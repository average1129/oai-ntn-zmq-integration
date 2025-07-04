/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file gNB_scheduler_phytest.c
 * \brief gNB scheduling procedures in phy_test mode
 * \author  Guy De Souza, G. Casati
 * \date 07/2018
 * \email: desouza@eurecom.fr, guido.casati@iis.fraunhofer.de
 * \version 1.0
 * @ingroup _mac
 */

#include "nr_mac_gNB.h"
#include "NR_MAC_gNB/mac_proto.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "executables/nr-softmodem.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "executables/softmodem-common.h"
#include "common/utils/nr/nr_common.h"

// #define UL_HARQ_PRINT
// #define ENABLE_MAC_PAYLOAD_DEBUG 1

/* This function checks whether the given Dl/UL slot is set
   in the input bitmap (per period), which is a mask indicating in which
   slot to transmit (among those available in the TDD configuration) */
static bool is_xlsch_in_slot(uint64_t bitmap, slot_t slot)
{
  AssertFatal(slot < 64, "Unable to handle periods with length larger than 64 slots in phy-test mode\n");
  return (bitmap >> slot) & 0x01;
}

uint32_t target_dl_mcs = 9;
uint32_t target_dl_Nl = 1;
uint32_t target_dl_bw = 50;
uint64_t dlsch_slot_bitmap = (1<<1);

/* schedules whole bandwidth for first user, all the time */
void nr_preprocessor_phytest(module_id_t module_id, frame_t frame, slot_t slot)
{
  gNB_MAC_INST *mac = RC.nrmac[module_id];
  /* already mutex protected: held in gNB_dlsch_ulsch_scheduler() */
  int slot_period = slot % mac->frame_structure.numb_slots_period;
  if (!is_xlsch_in_slot(dlsch_slot_bitmap, slot_period))
    return;
  NR_UE_info_t *UE = mac->UE_info.connected_ue_list[0];
  NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  NR_UE_DL_BWP_t *dl_bwp = &UE->current_DL_BWP;
  const int CC_id = 0;

  /* return if all DL HARQ processes wait for feedback */
  if (sched_ctrl->retrans_dl_harq.head == -1 && sched_ctrl->available_dl_harq.head == -1) {
    LOG_D(NR_MAC, "[UE %04x][%4d.%2d] UE has no free DL HARQ process, skipping\n", UE->rnti, frame, slot);
    return;
  }

  const int tda = get_dl_tda(mac, slot);
  NR_tda_info_t tda_info = get_dl_tda_info(dl_bwp,
                                           sched_ctrl->search_space->searchSpaceType->present,
                                           tda,
                                           scc->dmrs_TypeA_Position,
                                           1,
                                           TYPE_C_RNTI_,
                                           sched_ctrl->coreset->controlResourceSetId,
                                           false);
  if(!tda_info.valid_tda)
    return;

  sched_ctrl->sched_pdsch.tda_info = tda_info;
  sched_ctrl->sched_pdsch.time_domain_allocation = tda;

  /* find largest unallocated chunk */
  const int bwpSize = dl_bwp->BWPSize;
  const int BWPStart = dl_bwp->BWPStart;

  // TODO implement beam procedures for phy-test mode
  int beam = 0;

  int rbStart = 0;
  int rbSize = 0;
  if (target_dl_bw>bwpSize)
    target_dl_bw = bwpSize;
  uint16_t *vrb_map = mac->common_channels[CC_id].vrb_map[beam];
  /* loop ensures that we allocate exactly target_dl_bw, or return */
  while (true) {
    /* advance to first free RB */
    while (rbStart < bwpSize &&
           (vrb_map[rbStart + BWPStart]&SL_to_bitmap(tda_info.startSymbolIndex, tda_info.nrOfSymbols)))
      rbStart++;
    rbSize = 1;
    /* iterate until we are at target_dl_bw or no available RBs */
    while (rbStart + rbSize < bwpSize &&
           !(vrb_map[rbStart + rbSize + BWPStart]&SL_to_bitmap(tda_info.startSymbolIndex, tda_info.nrOfSymbols)) &&
           rbSize < target_dl_bw)
      rbSize++;
    /* found target_dl_bw? */
    if (rbSize == target_dl_bw)
      break;
    /* at end and below target_dl_bw? */
    if (rbStart + rbSize >= bwpSize)
      return;
    rbStart += rbSize;
  }

  sched_ctrl->num_total_bytes = 0;
  DevAssert(seq_arr_size(&sched_ctrl->lc_config) == 1);
  const nr_lc_config_t *c = seq_arr_at(&sched_ctrl->lc_config, 0);
  const int lcid = c->lcid;
  const uint16_t rnti = UE->rnti;
  /* update sched_ctrl->num_total_bytes so that postprocessor schedules data,
   * if available */
  sched_ctrl->rlc_status[lcid] = nr_mac_rlc_status_ind(rnti, frame, lcid);
  sched_ctrl->num_total_bytes += sched_ctrl->rlc_status[lcid].bytes_in_buffer;

  int CCEIndex = get_cce_index(mac,
                               CC_id, slot, UE->rnti,
                               &sched_ctrl->aggregation_level,
                               beam,
                               sched_ctrl->search_space,
                               sched_ctrl->coreset,
                               &sched_ctrl->sched_pdcch,
                               false,
                               0);
  AssertFatal(CCEIndex >= 0, "Could not find CCE for UE %04x\n", UE->rnti);

  NR_sched_pdsch_t *sched_pdsch = &sched_ctrl->sched_pdsch;
  if (sched_pdsch->dl_harq_pid == -1)
    sched_pdsch->dl_harq_pid = sched_ctrl->available_dl_harq.head;

  int alloc = -1;
  if (!get_FeedbackDisabled(UE->sc_info.downlinkHARQ_FeedbackDisabled_r17, sched_pdsch->dl_harq_pid)) {
    int r_pucch = nr_get_pucch_resource(sched_ctrl->coreset, UE->current_UL_BWP.pucch_Config, CCEIndex);
    alloc = nr_acknack_scheduling(mac, UE, frame, slot, 0, r_pucch, 0);
    if (alloc < 0) {
      LOG_D(NR_MAC, "Could not find PUCCH for UE %04x@%d.%d\n", rnti, frame, slot);
      return;
    }
  }

  sched_ctrl->cce_index = CCEIndex;

  fill_pdcch_vrb_map(mac,
                     CC_id,
                     &sched_ctrl->sched_pdcch,
                     CCEIndex,
                     sched_ctrl->aggregation_level,
                     beam);

  //AssertFatal(alloc,
  //            "could not find uplink slot for PUCCH (RNTI %04x@%d.%d)!\n",
  //            rnti, frame, slot);

  sched_pdsch->pucch_allocation = alloc;
  sched_pdsch->rbStart = rbStart;
  sched_pdsch->rbSize = rbSize;

  sched_pdsch->dmrs_parms = get_dl_dmrs_params(scc,
                                               dl_bwp,
                                               &tda_info,
                                               target_dl_Nl);

  sched_pdsch->mcs = target_dl_mcs;
  sched_pdsch->nrOfLayers = target_dl_Nl;
  sched_pdsch->Qm = nr_get_Qm_dl(sched_pdsch->mcs, dl_bwp->mcsTableIdx);
  sched_pdsch->R = nr_get_code_rate_dl(sched_pdsch->mcs, dl_bwp->mcsTableIdx);
  sched_ctrl->dl_bler_stats.mcs = target_dl_mcs; /* for logging output */
  sched_pdsch->tb_size = nr_compute_tbs(sched_pdsch->Qm,
                                        sched_pdsch->R,
                                        sched_pdsch->rbSize,
                                        tda_info.nrOfSymbols,
                                        sched_pdsch->dmrs_parms.N_PRB_DMRS * sched_pdsch->dmrs_parms.N_DMRS_SLOT,
                                        0 /* N_PRB_oh, 0 for initialBWP */,
                                        0 /* tb_scaling */,
                                        sched_pdsch->nrOfLayers)
                         >> 3;

  /* get the PID of a HARQ process awaiting retransmission, or -1 otherwise */
  sched_pdsch->dl_harq_pid = sched_ctrl->retrans_dl_harq.head;

  /* mark the corresponding RBs as used */
  for (int rb = 0; rb < sched_pdsch->rbSize; rb++)
    vrb_map[rb + sched_pdsch->rbStart + BWPStart] = SL_to_bitmap(tda_info.startSymbolIndex, tda_info.nrOfSymbols);

  if ((frame&127) == 0) LOG_D(MAC,"phytest: %d.%d DL mcs %d, DL rbStart %d, DL rbSize %d\n", frame, slot, sched_pdsch->mcs, rbStart,rbSize);
}

uint32_t target_ul_mcs = 9;
uint32_t target_ul_bw = 50;
uint32_t target_ul_Nl = 1;
uint64_t ulsch_slot_bitmap = (1 << 8);
bool nr_ul_preprocessor_phytest(module_id_t module_id, frame_t frame, slot_t slot)
{
  gNB_MAC_INST *nr_mac = RC.nrmac[module_id];
  /* already mutex protected: held in gNB_dlsch_ulsch_scheduler() */
  NR_COMMON_channels_t *cc = nr_mac->common_channels;
  NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
  NR_UE_info_t *UE = nr_mac->UE_info.connected_ue_list[0];

  AssertFatal(nr_mac->UE_info.connected_ue_list[1] == NULL,
              "cannot handle more than one UE\n");
  if (UE == NULL)
    return false;

  const int CC_id = 0;

  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  NR_UE_UL_BWP_t *ul_bwp = &UE->current_UL_BWP;

  /* return if all UL HARQ processes wait for feedback */
  if (sched_ctrl->retrans_ul_harq.head == -1 && sched_ctrl->available_ul_harq.head == -1) {
    LOG_D(NR_MAC, "[UE %04x][%4d.%2d] UE has no free UL HARQ process, skipping\n", UE->rnti, frame, slot);
    return false;
  }

  NR_PUSCH_TimeDomainResourceAllocationList_t *tdaList = get_ul_tdalist(ul_bwp,
                                                                        sched_ctrl->coreset->controlResourceSetId,
                                                                        sched_ctrl->search_space->searchSpaceType->present,
                                                                        TYPE_C_RNTI_);
  const int temp_tda = get_ul_tda(nr_mac, frame, slot);
  if (temp_tda < 0)
    return false;
  AssertFatal(temp_tda < tdaList->list.count, "time domain assignment %d >= %d\n", temp_tda, tdaList->list.count);
  const int mu = ul_bwp->scs;
  int K2 = get_K2(tdaList, temp_tda, mu, scc);
  int slots_frame = nr_mac->frame_structure.numb_slots_frame;
  const int sched_frame = (frame + (slot + K2) / slots_frame) % MAX_FRAME_NUMBER;
  const int sched_slot = (slot + K2) % slots_frame;
  const int tda = get_ul_tda(nr_mac, sched_frame, sched_slot);
  if (tda < 0)
    return false;
  AssertFatal(tda < tdaList->list.count,
              "time domain assignment %d >= %d\n",
              tda,
              tdaList->list.count);
  /* check if slot is UL, and that slot is 8 (assuming K2=6 because of UE
   * limitations).  Note that if K2 or the TDD configuration is changed, below
   * conditions might exclude each other and never be true */
  int slot_period = sched_slot % nr_mac->frame_structure.numb_slots_period;
  if (!is_xlsch_in_slot(ulsch_slot_bitmap, slot_period))
    return false;

  uint16_t rbStart = 0;
  uint16_t rbSize;

  const int bw = ul_bwp->BWPSize;
  const int BWPStart = ul_bwp->BWPStart;

  if (target_ul_bw>bw)
    rbSize = bw;
  else
    rbSize = target_ul_bw;

  NR_tda_info_t tda_info = get_ul_tda_info(ul_bwp,
                                           sched_ctrl->coreset->controlResourceSetId,
                                           sched_ctrl->search_space->searchSpaceType->present,
                                           TYPE_C_RNTI_,
                                           tda);
  if (!tda_info.valid_tda)
    return false;
  sched_ctrl->sched_pusch.tda_info = tda_info;
  sched_ctrl->sched_pusch.time_domain_allocation = tda;

  // TODO implement beam procedures for phy-test mode
  int beam = 0;

  const int buffer_index = ul_buffer_index(sched_frame, sched_slot, slots_frame, nr_mac->vrb_map_UL_size);
  uint16_t *vrb_map_UL = &nr_mac->common_channels[CC_id].vrb_map_UL[beam][buffer_index * MAX_BWP_SIZE];
  for (int i = rbStart; i < rbStart + rbSize; ++i) {
    if ((vrb_map_UL[i+BWPStart] & SL_to_bitmap(tda_info.startSymbolIndex, tda_info.nrOfSymbols)) != 0) {
      LOG_E(MAC, "%4d.%2d RB %d is already reserved, cannot schedule UE\n", frame, slot, i);
      return false;
    }
  }

  sched_ctrl->sched_pusch.slot = sched_slot;
  sched_ctrl->sched_pusch.frame = sched_frame;

  int CCEIndex = get_cce_index(nr_mac,
                               CC_id, slot, UE->rnti,
                               &sched_ctrl->aggregation_level,
                               beam,
                               sched_ctrl->search_space,
                               sched_ctrl->coreset,
                               &sched_ctrl->sched_pdcch,
                               false,
                               0);
  if (CCEIndex < 0) {
    LOG_E(MAC, "%s(): CCE list not empty, couldn't schedule PUSCH\n", __func__);
    return false;
  }

  sched_ctrl->cce_index = CCEIndex;

  const int mcs = target_ul_mcs;
  NR_sched_pusch_t *sched_pusch = &sched_ctrl->sched_pusch;
  sched_pusch->mcs = mcs;
  sched_ctrl->ul_bler_stats.mcs = mcs; /* for logging output */
  sched_pusch->rbStart = rbStart;
  sched_pusch->rbSize = rbSize;
  /* get the PID of a HARQ process awaiting retransmission, or -1 for "any new" */
  sched_pusch->ul_harq_pid = sched_ctrl->retrans_ul_harq.head;

  /* Calculate TBS from MCS */
  sched_pusch->nrOfLayers = target_ul_Nl;
  sched_pusch->R = nr_get_code_rate_ul(mcs, ul_bwp->mcs_table);
  sched_pusch->Qm = nr_get_Qm_ul(mcs, ul_bwp->mcs_table);
  if (ul_bwp->pusch_Config->tp_pi2BPSK
      && ((ul_bwp->mcs_table == 3 && mcs < 2) || (ul_bwp->mcs_table == 4 && mcs < 6))) {
    sched_pusch->R >>= 1;
    sched_pusch->Qm <<= 1;
  }

  NR_pusch_dmrs_t dmrs = get_ul_dmrs_params(scc,
                                            ul_bwp,
                                            &tda_info,
                                            sched_pusch->nrOfLayers);
  sched_ctrl->sched_pusch.dmrs_info = dmrs;

  sched_pusch->tb_size = nr_compute_tbs(sched_pusch->Qm,
                                        sched_pusch->R,
                                        sched_pusch->rbSize,
                                        tda_info.nrOfSymbols,
                                        dmrs.N_PRB_DMRS * dmrs.num_dmrs_symb,
                                        0, // nb_rb_oh
                                        0,
                                        sched_pusch->nrOfLayers /* NrOfLayers */)
                         >> 3;

  /* mark the corresponding RBs as used */
  fill_pdcch_vrb_map(nr_mac,
                     CC_id,
                     &sched_ctrl->sched_pdcch,
                     CCEIndex,
                     sched_ctrl->aggregation_level,
                     beam);

  for (int rb = rbStart; rb < rbStart + rbSize; rb++)
    vrb_map_UL[rb+BWPStart] |= SL_to_bitmap(tda_info.startSymbolIndex, tda_info.nrOfSymbols);
  return true;
}
