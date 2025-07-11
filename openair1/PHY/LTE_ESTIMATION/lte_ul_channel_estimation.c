/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
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

#include "PHY/defs_eNB.h"
#include "PHY/phy_extern.h"
#include "PHY/sse_intrin.h"
//#define DEBUG_CH
#include "common/utils/LOG/log.h"
#include "PHY/LTE_TRANSPORT/transport_common_proto.h"
#include "lte_estimation.h"
#include "openair1/PHY/LTE_TRANSPORT/transport_vars.h"
#include "openair1/PHY/LTE_TRANSPORT/pucch_extern.h"

// round(exp(sqrt(-1)*(pi/2)*[0:1:N-1]/N)*pow2(15))
static const int16_t ru_90[2 * 128] = {
    32767, 0,     32766, 402,   32758, 804,   32746, 1206,  32729, 1608,  32706, 2009,  32679, 2411,  32647, 2811,  32610, 3212,
    32568, 3612,  32522, 4011,  32470, 4410,  32413, 4808,  32352, 5205,  32286, 5602,  32214, 5998,  32138, 6393,  32058, 6787,
    31972, 7180,  31881, 7571,  31786, 7962,  31686, 8351,  31581, 8740,  31471, 9127,  31357, 9512,  31238, 9896,  31114, 10279,
    30986, 10660, 30853, 11039, 30715, 11417, 30572, 11793, 30425, 12167, 30274, 12540, 30118, 12910, 29957, 13279, 29792, 13646,
    29622, 14010, 29448, 14373, 29269, 14733, 29086, 15091, 28899, 15447, 28707, 15800, 28511, 16151, 28311, 16500, 28106, 16846,
    27897, 17190, 27684, 17531, 27467, 17869, 27246, 18205, 27020, 18538, 26791, 18868, 26557, 19195, 26320, 19520, 26078, 19841,
    25833, 20160, 25583, 20475, 25330, 20788, 25073, 21097, 24812, 21403, 24548, 21706, 24279, 22006, 24008, 22302, 23732, 22595,
    23453, 22884, 23170, 23170, 22884, 23453, 22595, 23732, 22302, 24008, 22006, 24279, 21706, 24548, 21403, 24812, 21097, 25073,
    20788, 25330, 20475, 25583, 20160, 25833, 19841, 26078, 19520, 26320, 19195, 26557, 18868, 26791, 18538, 27020, 18205, 27246,
    17869, 27467, 17531, 27684, 17190, 27897, 16846, 28106, 16500, 28311, 16151, 28511, 15800, 28707, 15447, 28899, 15091, 29086,
    14733, 29269, 14373, 29448, 14010, 29622, 13646, 29792, 13279, 29957, 12910, 30118, 12540, 30274, 12167, 30425, 11793, 30572,
    11417, 30715, 11039, 30853, 10660, 30986, 10279, 31114, 9896,  31238, 9512,  31357, 9127,  31471, 8740,  31581, 8351,  31686,
    7962,  31786, 7571,  31881, 7180,  31972, 6787,  32058, 6393,  32138, 5998,  32214, 5602,  32286, 5205,  32352, 4808,  32413,
    4410,  32470, 4011,  32522, 3612,  32568, 3212,  32610, 2811,  32647, 2411,  32679, 2009,  32706, 1608,  32729, 1206,  32746,
    804,   32758, 402,   32766};

static const int16_t ru_90c[2 * 128] = {
    32767, 0,      32766, -402,   32758, -804,   32746, -1206,  32729, -1608,  32706, -2009,  32679, -2411,  32647, -2811,
    32610, -3212,  32568, -3612,  32522, -4011,  32470, -4410,  32413, -4808,  32352, -5205,  32286, -5602,  32214, -5998,
    32138, -6393,  32058, -6787,  31972, -7180,  31881, -7571,  31786, -7962,  31686, -8351,  31581, -8740,  31471, -9127,
    31357, -9512,  31238, -9896,  31114, -10279, 30986, -10660, 30853, -11039, 30715, -11417, 30572, -11793, 30425, -12167,
    30274, -12540, 30118, -12910, 29957, -13279, 29792, -13646, 29622, -14010, 29448, -14373, 29269, -14733, 29086, -15091,
    28899, -15447, 28707, -15800, 28511, -16151, 28311, -16500, 28106, -16846, 27897, -17190, 27684, -17531, 27467, -17869,
    27246, -18205, 27020, -18538, 26791, -18868, 26557, -19195, 26320, -19520, 26078, -19841, 25833, -20160, 25583, -20475,
    25330, -20788, 25073, -21097, 24812, -21403, 24548, -21706, 24279, -22006, 24008, -22302, 23732, -22595, 23453, -22884,
    23170, -23170, 22884, -23453, 22595, -23732, 22302, -24008, 22006, -24279, 21706, -24548, 21403, -24812, 21097, -25073,
    20788, -25330, 20475, -25583, 20160, -25833, 19841, -26078, 19520, -26320, 19195, -26557, 18868, -26791, 18538, -27020,
    18205, -27246, 17869, -27467, 17531, -27684, 17190, -27897, 16846, -28106, 16500, -28311, 16151, -28511, 15800, -28707,
    15447, -28899, 15091, -29086, 14733, -29269, 14373, -29448, 14010, -29622, 13646, -29792, 13279, -29957, 12910, -30118,
    12540, -30274, 12167, -30425, 11793, -30572, 11417, -30715, 11039, -30853, 10660, -30986, 10279, -31114, 9896,  -31238,
    9512,  -31357, 9127,  -31471, 8740,  -31581, 8351,  -31686, 7962,  -31786, 7571,  -31881, 7180,  -31972, 6787,  -32058,
    6393,  -32138, 5998,  -32214, 5602,  -32286, 5205,  -32352, 4808,  -32413, 4410,  -32470, 4011,  -32522, 3612,  -32568,
    3212,  -32610, 2811,  -32647, 2411,  -32679, 2009,  -32706, 1608,  -32729, 1206,  -32746, 804,   -32758, 402,   -32766};

#define SCALE 0x3FFF

int32_t lte_ul_channel_estimation(LTE_DL_FRAME_PARMS *frame_parms,
                                  L1_rxtx_proc_t *proc,
                                  LTE_eNB_ULSCH_t *ulsch,
                                  c16_t **ul_ch_estimates,
                                  c16_t **ul_ch_estimates_time,
                                  c16_t **rxdataF_ext,
                                  module_id_t UE_id,
                                  unsigned char l,
                                  unsigned char Ns)
{
  AssertFatal(ul_ch_estimates != NULL, "ul_ch_estimates is null ");
  AssertFatal(ul_ch_estimates_time != NULL, "ul_ch_estimates_time is null\n");
  int subframe = proc->subframe_rx;

  uint8_t harq_pid;

  int16_t delta_phase = 0;
  int16_t current_phase1,current_phase2;
  uint16_t aa,Msc_RS,Msc_RS_idx;
  uint16_t *Msc_idx_ptr;
  int k,pilot_pos1 = 3 - frame_parms->Ncp, pilot_pos2 = 10 - 2*frame_parms->Ncp;
  c16_t *ul_ch1 = NULL, *ul_ch2 = NULL;
  //uint8_t nb_antennas_rx = frame_parms->nb_antenna_ports_eNB;
  uint8_t nb_antennas_rx = frame_parms->nb_antennas_rx;
  uint8_t cyclic_shift;
  uint32_t alpha_ind;
  uint32_t u=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[Ns+(subframe<<1)];
  uint32_t v=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[Ns+(subframe<<1)];
  int symbol_offset,i;
  // debug_msg("lte_ul_channel_estimation: cyclic shift %d\n",cyclicShift);
  simde__m128i *rxdataF128, *ul_ref128, *ul_ch128;
  c16_t temp_in_ifft_0[2048 * 2] __attribute__((aligned(32)));

  if (ulsch->ue_type > 0) harq_pid = 0;
  else {
    harq_pid = subframe2harq_pid(frame_parms,proc->frame_rx,subframe);
  }

  uint16_t N_rb_alloc = ulsch->harq_processes[harq_pid]->nb_rb;
  c16_t tmp_estimates[N_rb_alloc * 12] __attribute__((aligned(32)));
  Msc_RS = N_rb_alloc*12;
  cyclic_shift = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  ulsch->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)+Ns]) % 12;
  Msc_idx_ptr = (uint16_t *) bsearch(&Msc_RS, dftsizes, 34, sizeof(uint16_t), compareints);

  if (Msc_idx_ptr)
    Msc_RS_idx = Msc_idx_ptr - dftsizes;
  else {
    LOG_E(PHY,"lte_ul_channel_estimation: index for Msc_RS=%d not found\n",Msc_RS);
    return(-1);
  }

  LOG_D(PHY,"subframe %d, Ns %d, l %d, Msc_RS = %d, Msc_RS_idx = %d, u %d, v %d, cyclic_shift %d\n",subframe,Ns,l,Msc_RS, Msc_RS_idx,u,v,cyclic_shift);
#ifdef DEBUG_CH

  if (Ns==0)
    LOG_M("drs_seq0.m","drsseq0",ul_ref_sigs_rx[u][v][Msc_RS_idx],2*Msc_RS,2,0);
  else
    LOG_M("drs_seq1.m","drsseq1",ul_ref_sigs_rx[u][v][Msc_RS_idx],2*Msc_RS,2,0);

#endif

  if (l == (3 - frame_parms->Ncp)) {
    symbol_offset = frame_parms->N_RB_UL*12*(l+((7-frame_parms->Ncp)*(Ns&1)));

    for (aa = 0; aa < nb_antennas_rx; aa++) {
      rxdataF128 = (simde__m128i *)&rxdataF_ext[aa][symbol_offset];
      ul_ch128 = (simde__m128i *)&ul_ch_estimates[aa][symbol_offset];
      ul_ref128 = (simde__m128i *)ul_ref_sigs_rx[u][v][Msc_RS_idx];

      for (i = 0; i < Msc_RS >> 2; i++) {
        ul_ch128[i] = oai_mm_cpx_mult_conj(ul_ref128[i], rxdataF128[i], 15);
      }

      alpha_ind = 0;

      if((cyclic_shift != 0)) {
        // Compensating for the phase shift introduced at the transmitte
#ifdef DEBUG_CH
        LOG_M("drs_est_pre.m","drsest_pre",ul_ch_estimates[0],300*12,1,1);
#endif

        for(i=symbol_offset; i<symbol_offset+Msc_RS; i++) {
          c16_t alpha = (c16_t){alphaTBL_re[alpha_ind], alphaTBL_im[alpha_ind]};
          ul_ch_estimates[aa][i] = c16MulConjShift(alpha, ul_ch_estimates[aa][i], 15);
          alpha_ind+=cyclic_shift;

          if (alpha_ind>11)
            alpha_ind-=12;
        }

#ifdef DEBUG_CH
        LOG_M("drs_est_post.m","drsest_post",ul_ch_estimates[0],300*12,1,1);
#endif
      }

      // Convert to time domain for visualization
      memset(temp_in_ifft_0,0,frame_parms->ofdm_symbol_size*sizeof(int32_t));

      for(i=0; i<Msc_RS; i++)
        temp_in_ifft_0[i] = ul_ch_estimates[aa][symbol_offset + i];
      int len;
      switch(frame_parms->N_RB_DL) {
        case 6:
          len = 128;
          break;
        case 25:
          len = 512;
          break;
        case 50:
          len = 1024;
          break;
        case 100:
          len = 2048;
          break;
        default:
          LOG_E(PHY, "Unknown N_RB_DL %d\n", frame_parms->N_RB_DL);
          return -1;
      }
      idft(get_idft(len), (int16_t *)temp_in_ifft_0, (int16_t *)ul_ch_estimates_time[aa], 1);
#if T_TRACER

      if (aa == 0)
        T(T_ENB_PHY_UL_CHANNEL_ESTIMATE, T_INT(0), T_INT(ulsch->rnti),
          T_INT(proc->frame_rx), T_INT(subframe),
          T_INT(0), T_BUFFER(ul_ch_estimates_time[0], 512  * 4));

#endif
#ifdef DEBUG_CH

      if (aa==1) {
        if (Ns == 0) {
          LOG_M("rxdataF_ext.m","rxF_ext",&rxdataF_ext[aa][symbol_offset],512*2,2,1);
          LOG_M("tmpin_ifft.m","drs_in",temp_in_ifft_0,512,1,1);
          LOG_M("drs_est0.m","drs0",ul_ch_estimates_time[aa],512,1,1);
        } else
          LOG_M("drs_est1.m","drs1",ul_ch_estimates_time[aa],512,1,1);
      }

#endif

      if (Ns&1) {//we are in the second slot of the sub-frame, so do the interpolation
        ul_ch1 = &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*pilot_pos1];
        ul_ch2 = &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*pilot_pos2];
        // Estimation of phase difference between the 2 channel estimates
        delta_phase = lte_ul_freq_offset_estimation(frame_parms,
                      ul_ch_estimates[aa],
                      N_rb_alloc);
        // negative phase index indicates negative Im of ru
        //    msg("delta_phase: %d\n",delta_phase);
#ifdef DEBUG_CH
        LOG_D(PHY,"lte_ul_channel_estimation: ul_ch1 = %p, ul_ch2 = %p, pilot_pos1=%d, pilot_pos2=%d\n",ul_ch1, ul_ch2, pilot_pos1,pilot_pos2);
#endif

        for (k=0; k<frame_parms->symbols_per_tti; k++) {
          // we scale alpha and beta by SCALE (instead of 0x7FFF) to avoid overflows
          //      alpha = (int16_t) (((int32_t) SCALE * (int32_t) (pilot_pos2-k))/(pilot_pos2-pilot_pos1));
          //      beta  = (int16_t) (((int32_t) SCALE * (int32_t) (k-pilot_pos1))/(pilot_pos2-pilot_pos1));
#ifdef DEBUG_CH
          LOG_D(PHY,"lte_ul_channel_estimation: k=%d\n",k);
#endif
          //symbol_offset_subframe = frame_parms->N_RB_UL*12*k;

          // interpolate between estimates
          if ((k != pilot_pos1) && (k != pilot_pos2))  {
            //          multadd_complex_vector_real_scalar((int16_t*) ul_ch1,alpha,(int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],1,Msc_RS);
            //          multadd_complex_vector_real_scalar((int16_t*) ul_ch2,beta ,(int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],0,Msc_RS);
            //          multadd_complex_vector_real_scalar((int16_t*) ul_ch1,SCALE,(int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],1,Msc_RS);
            //          multadd_complex_vector_real_scalar((int16_t*) ul_ch2,SCALE,(int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],0,Msc_RS);
            //          msg("phase = %d\n",ru[2*cmax(((delta_phase/7)*(k-3)),0)]);
            // the phase is linearly interpolated
            current_phase1 = (delta_phase/7)*(k-pilot_pos1);
            current_phase2 = (delta_phase/7)*(k-pilot_pos2);
            //          msg("sym: %d, current_phase1: %d, current_phase2: %d\n",k,current_phase1,current_phase2);
            // set the right quadrant
            c16_t *ru1 = (c16_t *)(current_phase1 > 0 ? ru_90 : ru_90c);
            c16_t *ru2 = (c16_t *)(current_phase2 > 0 ? ru_90 : ru_90c);
            // take absolute value and clip
            current_phase1 = cmin(abs(current_phase1),127);
            current_phase2 = cmin(abs(current_phase2), 127);
            // rotate channel estimates by estimated phase
            rotate_cpx_vector(ul_ch1, &ru1[current_phase1], &ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * k], Msc_RS, 15);
            rotate_cpx_vector(ul_ch2, &ru2[current_phase2], tmp_estimates, Msc_RS, 15);
            // Combine the two rotated estimates
            mult_complex_vector_real_scalar(&ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * k],
                                            SCALE,
                                            &ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * k],
                                            Msc_RS);
            multadd_complex_vector_real_scalar(tmp_estimates, SCALE, &ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * k], Msc_RS);
            /*
            if ((k<pilot_pos1) || ((k>pilot_pos2))) {

                    multadd_complex_vector_real_scalar((int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],SCALE>>1,(int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],1,Msc_RS);

                    multadd_complex_vector_real_scalar((int16_t*) &tmp_estimates[0],SCALE>>1,(int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],0,Msc_RS);

            } else {

                    multadd_complex_vector_real_scalar((int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],SCALE>>1,(int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],1,Msc_RS);

                    multadd_complex_vector_real_scalar((int16_t*) &tmp_estimates[0],SCALE>>1,(int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],0,Msc_RS);

                    //              multadd_complex_vector_real_scalar((int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],alpha,(int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],1,Msc_RS);

                    //              multadd_complex_vector_real_scalar((int16_t*) &tmp_estimates[0],beta ,(int16_t*) &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],0,Msc_RS);

            }
            */
            //      memcpy(&ul_ch_estimates[aa][frame_parms->N_RB_UL*12*k],ul_ch1,Msc_RS*sizeof(int32_t));
          }
        } //for(k=...

        // because of the scaling of alpha and beta we also need to scale the final channel estimate at the pilot positions
        //    multadd_complex_vector_real_scalar((int16_t*) ul_ch1,SCALE,(int16_t*) ul_ch1,1,Msc_RS);
        //    multadd_complex_vector_real_scalar((int16_t*) ul_ch2,SCALE,(int16_t*) ul_ch2,1,Msc_RS);
      } //if (Ns&1)
    } // for(aa=...
  } //if(l==...

  return(0);
}

int32_t lte_ul_channel_estimation_RRU(LTE_DL_FRAME_PARMS *frame_parms,
                                      c16_t **ul_ch_estimates,
                                      c16_t **ul_ch_estimates_time,
                                      c16_t **rxdataF_ext,
                                      int N_rb_alloc,
                                      int frame_rx,
                                      int subframe_rx,
                                      uint32_t u,
                                      uint32_t v,
                                      uint32_t cyclic_shift,
                                      unsigned char l,
                                      int interpolate,
                                      uint16_t rnti)
{
  int16_t delta_phase = 0;
  int16_t current_phase1,current_phase2;
  uint16_t aa,Msc_RS,Msc_RS_idx;
  uint16_t *Msc_idx_ptr;
  int k,pilot_pos1 = 3 - frame_parms->Ncp, pilot_pos2 = 10 - 2*frame_parms->Ncp;
  c16_t *ul_ch1 = NULL, *ul_ch2 = NULL;
  uint8_t nb_antennas_rx = frame_parms->nb_antennas_rx;
  uint32_t alpha_ind;
  c16_t tmp_estimates[N_rb_alloc * 12] __attribute__((aligned(16)));
  int symbol_offset,i;
  // debug_msg("lte_ul_channel_estimation_RRU: cyclic shift %d\n",cyclicShift);
  simde__m128i *rxdataF128, *ul_ref128, *ul_ch128;
  c16_t temp_in_ifft_0[2048 * 2] __attribute__((aligned(32)));
  AssertFatal(l==pilot_pos1 || l==pilot_pos2,"%d is not a valid symbol for DMRS, should be %d or %d\n",
              l,pilot_pos1,pilot_pos2);
  Msc_RS = N_rb_alloc*12;
  /*

  */
  Msc_idx_ptr = (uint16_t *) bsearch(&Msc_RS, dftsizes, 33, sizeof(uint16_t), compareints);

  if (Msc_idx_ptr)
    Msc_RS_idx = Msc_idx_ptr - dftsizes;
  else {
    LOG_E(PHY,"lte_ul_channel_estimation_RRU: index for Msc_RS=%d not found\n",Msc_RS);
    return(-1);
  }

  LOG_D(PHY,"subframe %d, l %d, Msc_RS = %d, Msc_RS_idx = %d, u %d, v %d, cyclic_shift %d\n",subframe_rx,l,Msc_RS, Msc_RS_idx,u,v,cyclic_shift);
#ifdef DEBUG_CH

  if (l==pilot_pos1)
    write_output("drs_seq0.m","drsseq0",ul_ref_sigs_rx[u][v][Msc_RS_idx],Msc_RS,1,0);
  else
    write_output("drs_seq1.m","drsseq1",ul_ref_sigs_rx[u][v][Msc_RS_idx],Msc_RS,1,0);

#endif
  symbol_offset = frame_parms->N_RB_UL*12*l;

  for (aa = 0; aa < nb_antennas_rx; aa++) {
    rxdataF128 = (simde__m128i *)&rxdataF_ext[aa][symbol_offset];
    ul_ch128 = (simde__m128i *)&ul_ch_estimates[aa][symbol_offset];
    ul_ref128 = (simde__m128i *)ul_ref_sigs_rx[u][v][Msc_RS_idx];

    for (i = 0; i < Msc_RS >> 2; i++) {
      ul_ch128[i] = oai_mm_cpx_mult_conj(ul_ref128[i], rxdataF128[i], 15);
    }

    alpha_ind = 0;

    if((cyclic_shift != 0)) {
      // Compensating for the phase shift introduced at the transmitte
#ifdef DEBUG_CH
      write_output("drs_est_pre.m","drsest_pre",ul_ch_estimates[0],300*12,1,1);
#endif

      for(i=symbol_offset; i<symbol_offset+Msc_RS; i++) {
        c16_t alpha = (c16_t){alphaTBL_re[alpha_ind], alphaTBL_im[alpha_ind]};
        ul_ch_estimates[aa][i] = c16MulConjShift(alpha, ul_ch_estimates[aa][i], 15);
        alpha_ind+=cyclic_shift;

        if (alpha_ind>11)
          alpha_ind-=12;
      }

#ifdef DEBUG_CH
      write_output("drs_est_post.m","drsest_post",ul_ch_estimates[0],300*12,1,1);
#endif
    }

    if (ul_ch_estimates_time && ul_ch_estimates_time[aa]) {
      // Convert to time domain for visualization
      memset(temp_in_ifft_0,0,frame_parms->ofdm_symbol_size*sizeof(int32_t));

      for(i=0; i<Msc_RS; i++)
        temp_in_ifft_0[i] = ul_ch_estimates[aa][symbol_offset + i];
      int len;
      switch(frame_parms->N_RB_DL) {
        case 6:
          len = 128;
          break;
        case 25:
          len = 512;
          break;
        case 50:
          len = 1024;
          break;
        case 100:
          len = 2048;
          break;
        default:
          LOG_E(PHY, "Unknown N_RB_DL %d\n", frame_parms->N_RB_DL);
          return -1;
      }
      idft(get_idft(len), (int16_t *)temp_in_ifft_0, (int16_t *)ul_ch_estimates_time[aa], 1);
#if T_TRACER

      if (aa == 0)
        T(T_ENB_PHY_UL_CHANNEL_ESTIMATE, T_INT(0), T_INT(rnti),
          T_INT(frame_rx), T_INT(subframe_rx),
          T_INT(0), T_BUFFER(ul_ch_estimates_time[0], 512  * 4));

#endif
    }

#ifdef DEBUG_CH

    if (aa==1) {
      if (l == pilot_pos1) {
        write_output("rxdataF_ext.m","rxF_ext",&rxdataF_ext[aa][symbol_offset],512*2,2,1);
        write_output("tmpin_ifft.m","drs_in",temp_in_ifft_0,512,1,1);

        if (ul_ch_estimates_time[aa]) write_output("drs_est0.m","drs0",ul_ch_estimates_time[aa],512,1,1);
      } else if (ul_ch_estimates_time[aa]) write_output("drs_est1.m","drs1",ul_ch_estimates_time[aa],512,1,1);
    }

#endif

    if (l==pilot_pos2 && interpolate==1) {//we are in the second slot of the sub-frame, so do the interpolation
      ul_ch1 = &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*pilot_pos1];
      ul_ch2 = &ul_ch_estimates[aa][frame_parms->N_RB_UL*12*pilot_pos2];
      // Estimation of phase difference between the 2 channel estimates
      delta_phase = lte_ul_freq_offset_estimation(frame_parms,
                    ul_ch_estimates[aa],
                    N_rb_alloc);
      // negative phase index indicates negative Im of ru
      //    msg("delta_phase: %d\n",delta_phase);
#ifdef DEBUG_CH
      LOG_D(PHY,"lte_ul_channel_estimation_RRU: ul_ch1 = %p, ul_ch2 = %p, pilot_pos1=%d, pilot_pos2=%d\n",ul_ch1, ul_ch2, pilot_pos1,pilot_pos2);
#endif

      for (k=0; k<frame_parms->symbols_per_tti; k++) {
#ifdef DEBUG_CH
        //  LOG_D(PHY,"lte_ul_channel_estimation: k=%d, alpha = %d, beta = %d\n",k,alpha,beta);
#endif
        //symbol_offset_subframe = frame_parms->N_RB_UL*12*k;

        // interpolate between estimates
        if ((k != pilot_pos1) && (k != pilot_pos2))  {
          // the phase is linearly interpolated
          current_phase1 = (delta_phase/7)*(k-pilot_pos1);
          current_phase2 = (delta_phase/7)*(k-pilot_pos2);
          //          msg("sym: %d, current_phase1: %d, current_phase2: %d\n",k,current_phase1,current_phase2);
          // set the right quadrant
          c16_t *ru1 = (c16_t *)(current_phase1 > 0 ? ru_90 : ru_90c);
          c16_t *ru2 = (c16_t *)(current_phase2 > 0 ? ru_90 : ru_90c);
          // take absolute value and clip
          current_phase1 = cmin(abs(current_phase1),127);
          current_phase2 = cmin(abs(current_phase2), 127);
          // rotate channel estimates by estimated phase
          rotate_cpx_vector(ul_ch1, &ru1[current_phase1], &ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * k], Msc_RS, 15);
          rotate_cpx_vector(ul_ch2, &ru2[current_phase2], tmp_estimates, Msc_RS, 15);
          // Combine the two rotated estimates
          mult_complex_vector_real_scalar(&ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * k],
                                          SCALE,
                                          &ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * k],
                                          Msc_RS);
          multadd_complex_vector_real_scalar(tmp_estimates, SCALE, &ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * k], Msc_RS);
        }
      } //for(k=...

      // because of the scaling of alpha and beta we also need to scale the final channel estimate at the pilot positions
      //    multadd_complex_vector_real_scalar((int16_t*) ul_ch1,SCALE,(int16_t*) ul_ch1,1,Msc_RS);
      //    multadd_complex_vector_real_scalar((int16_t*) ul_ch2,SCALE,(int16_t*) ul_ch2,1,Msc_RS);
    } //if (Ns&1 && interpolate==1)
    else if (interpolate == 0 && l == pilot_pos1)
      for (k=0; k<frame_parms->symbols_per_tti>>1; k++) {
        if (k==pilot_pos1) k++;

        memcpy((void *)&ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * k],
               (void *)&ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * pilot_pos1],
               frame_parms->N_RB_UL * 12 * sizeof(int));
      } else if (interpolate == 0 && l == pilot_pos2) {
      for (k=0; k<frame_parms->symbols_per_tti>>1; k++) {
        if (k==pilot_pos2) k++;

        memcpy((void *)&ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * (k + (frame_parms->symbols_per_tti >> 1))],
               (void *)&ul_ch_estimates[aa][frame_parms->N_RB_UL * 12 * pilot_pos2],
               frame_parms->N_RB_UL * 12 * sizeof(int));
      }

      delta_phase = lte_ul_freq_offset_estimation(frame_parms,
                    ul_ch_estimates[aa],
                    N_rb_alloc);
      LOG_D(PHY,"delta_phase = %d\n",delta_phase);
    }
  } // for(aa=...

  return(0);
}

extern uint16_t transmission_offset_tdd[16];
//#define DEBUG_SRS

int32_t lte_srs_channel_estimation(LTE_DL_FRAME_PARMS *frame_parms,
                                   LTE_eNB_COMMON *common_vars,
                                   LTE_eNB_SRS *srs_vars,
                                   SOUNDINGRS_UL_CONFIG_DEDICATED *soundingrs_ul_config_dedicated,
                                   unsigned char subframe,
                                   unsigned char eNB_id) {
  int aa;
  int N_symb,symbol;
  uint8_t nb_antennas_rx = frame_parms->nb_antennas_rx;
#ifdef DEBUG_SRS
  char fname[40], vname[40];
#endif
  //uint8_t Ssrs  = frame_parms->soundingrs_ul_config_common.srs_SubframeConfig;
  //uint8_t T_SFC = (Ssrs<=7 ? 5 : 10);
  N_symb = 2*7-frame_parms->Ncp;
  symbol = N_symb-1; //SRS is always in last symbol of subframe

  /*
     msg("SRS channel estimation eNB %d, subframs %d, %d %d %d %d %d\n",eNB_id,sub_frame_number,
     SRS_parms->Csrs,
     SRS_parms->Bsrs,
     SRS_parms->kTC,
     SRS_parms->n_RRC,
     SRS_parms->Ssrs);
  */

  //if ((1<<(sub_frame_number%T_SFC))&transmission_offset_tdd[Ssrs]) {

  if (generate_srs(frame_parms,
                   soundingrs_ul_config_dedicated,
                   &srs_vars->srs[eNB_id],
                   0x7FFF,
                   subframe)==-1) {
    LOG_E(PHY,"lte_srs_channel_estimation: Error in generate_srs\n");
    return(-1);
  }

  for (aa=0; aa<nb_antennas_rx; aa++) {
#ifdef DEBUG_SRS
    LOG_E(PHY,"SRS channel estimation eNB %d, subframs %d, aarx %d, %p, %p, %p\n",eNB_id,sub_frame_number,aa,
          &common_vars->rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],
          srs_vars->srs,
          srs_vars->srs_ch_estimates[aa]);
#endif
    //LOG_M("eNB_rxF.m","rxF",&common_vars->rxdataF[0][aa][2*frame_parms->ofdm_symbol_size*symbol],2*(frame_parms->ofdm_symbol_size),2,1);
    //LOG_M("eNB_srs.m","srs_eNB",common_vars->srs,(frame_parms->ofdm_symbol_size),1,1);
    mult_cpx_conj_vector((c16_t *)&common_vars->rxdataF[aa][2 * frame_parms->ofdm_symbol_size * symbol],
                         (c16_t *)srs_vars->srs,
                         (c16_t *)srs_vars->srs_ch_estimates[aa],
                         frame_parms->ofdm_symbol_size,
                         15);
#ifdef DEBUG_SRS
    sprintf(fname,"srs_ch_est%d.m",aa);
    sprintf(vname,"srs_est%d",aa);
    LOG_M(fname,vname,srs_vars->srs_ch_estimates[aa],frame_parms->ofdm_symbol_size,1,1);
#endif
  }

  /*
    else {
    for (aa=0;aa<nb_antennas_rx;aa++)
    bzero(srs_vars->srs_ch_estimates[eNB_id][aa],frame_parms->ofdm_symbol_size*sizeof(int));
    }
  */
  return(0);
}

int16_t lte_ul_freq_offset_estimation(LTE_DL_FRAME_PARMS *frame_parms, c16_t *ul_ch_estimates, uint16_t nb_rb)
{
  int a_idx = 64;
  uint8_t conj_flag = 0;
  uint8_t output_shift;
  int pilot_pos1 = 3 - frame_parms->Ncp;
  int pilot_pos2 = 10 - 2*frame_parms->Ncp;
  simde__m128i *ul_ch1 = (simde__m128i *)&ul_ch_estimates[pilot_pos1 * frame_parms->N_RB_UL * 12];
  simde__m128i *ul_ch2 = (simde__m128i *)&ul_ch_estimates[pilot_pos2 * frame_parms->N_RB_UL * 12];
  int32_t avg[2];
  int16_t Ravg[2];
  Ravg[0]=0;
  Ravg[1]=0;
  int16_t iv, rv, phase_idx = 0;
  simde__m128 avg128U1, avg128U2;

  // round(tan((pi/4)*[1:1:N]/N)*pow2(15))
  int16_t alpha[128] = {201, 402, 603, 804, 1006, 1207, 1408, 1610, 1811, 2013, 2215, 2417, 2619, 2822, 3024, 3227, 3431, 3634, 3838, 4042, 4246, 4450, 4655, 4861, 5066, 5272, 5479, 5686, 5893, 6101, 6309, 6518, 6727, 6937, 7147, 7358, 7570, 7782, 7995, 8208, 8422, 8637, 8852, 9068, 9285, 9503, 9721, 9940, 10160, 10381, 10603, 10825, 11049, 11273, 11498, 11725, 11952, 12180, 12410, 12640, 12872, 13104, 13338, 13573, 13809, 14046, 14285, 14525, 14766, 15009, 15253, 15498, 15745, 15993, 16243, 16494, 16747, 17001, 17257, 17515, 17774, 18035, 18298, 18563, 18829, 19098, 19368, 19640, 19915, 20191, 20470, 20750, 21033, 21318, 21605, 21895, 22187, 22481, 22778, 23078, 23380, 23685, 23992, 24302, 24615, 24931, 25250, 25572, 25897, 26226, 26557, 26892, 27230, 27572, 27917, 28266, 28618, 28975, 29335, 29699, 30067, 30440, 30817, 31198, 31583, 31973, 32368, 32767};
  // compute log2_maxh (output_shift)
  avg128U1 = simde_mm_setzero_ps();
  avg128U2 = simde_mm_setzero_ps();

  for (int rb=0; rb<nb_rb; rb++) {
    avg128U1 = simde_mm_add_ps(avg128U1, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(ul_ch1[0], ul_ch1[0])));
    avg128U1 = simde_mm_add_ps(avg128U1, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(ul_ch1[1], ul_ch1[1])));
    avg128U1 = simde_mm_add_ps(avg128U1, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(ul_ch1[2], ul_ch1[2])));

    avg128U2 = simde_mm_add_ps(avg128U2, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(ul_ch2[0], ul_ch2[0])));
    avg128U2 = simde_mm_add_ps(avg128U2, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(ul_ch2[1], ul_ch2[1])));
    avg128U2 = simde_mm_add_ps(avg128U2, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(ul_ch2[2], ul_ch2[2])));

    ul_ch1+=3;
    ul_ch2+=3;
  }
  
  // Horizontal add
  avg[0] = (int)( (((float*)&avg128U1)[0] +
                   ((float*)&avg128U1)[1] +
                   ((float*)&avg128U1)[2] +
                   ((float*)&avg128U1)[3])/(float)(nb_rb*12) );

  avg[1] = (int)( (((float*)&avg128U2)[0] +
                   ((float*)&avg128U2)[1] +
                   ((float*)&avg128U2)[2] +
                   ((float*)&avg128U2)[3])/(float)(nb_rb*12) );

  //    msg("avg0 = %d, avg1 = %d\n",avg[0],avg[1]);
  avg[0] = cmax(avg[0],avg[1]);
  avg[1] = log2_approx(avg[0]);
  output_shift = cmax(0,avg[1]-10);
  //output_shift  = (log2_approx(avg[0])/2)+ log2_approx(frame_parms->nb_antennas_rx-1)+1;
  //    msg("avg= %d, shift = %d\n",avg[0],output_shift);
  ul_ch1 = (simde__m128i *)&ul_ch_estimates[pilot_pos1 * frame_parms->N_RB_UL * 12];
  ul_ch2 = (simde__m128i *)&ul_ch_estimates[pilot_pos2 * frame_parms->N_RB_UL * 12];

  // correlate and average the 2 channel estimates ul_ch1*ul_ch2
  for (int rb=0; rb<nb_rb; rb++) {
    simde__m128i R[3];
    R[0] = oai_mm_cpx_mult_conj(ul_ch1[0], ul_ch2[0], output_shift);
    R[1] = oai_mm_cpx_mult_conj(ul_ch1[1], ul_ch2[1], output_shift);
    R[2] = oai_mm_cpx_mult_conj(ul_ch1[2], ul_ch2[2], output_shift);

    // Horizontal add
    R[0] = simde_mm_add_epi16(simde_mm_srai_epi16(R[0], 1), simde_mm_srai_epi16(R[1], 1));
    R[0] = simde_mm_add_epi16(simde_mm_srai_epi16(R[0], 1), simde_mm_srai_epi16(R[2], 1));
    Ravg[0] += (((short *)&R)[0] +
                ((short *)&R)[2] +
                ((short *)&R)[4] +
                ((short *)&R)[6])/(nb_rb*4);
    Ravg[1] += (((short *)&R)[1] +
                ((short *)&R)[3] +
                ((short *)&R)[5] +
                ((short *)&R)[7])/(nb_rb*4);
    ul_ch1+=3;
    ul_ch2+=3;
  }

  // phase estimation on Ravg
  //   Ravg[0] = 56;
  //   Ravg[1] = 0;
  rv = Ravg[0];
  iv = Ravg[1];

  if (iv<0)
    iv = -Ravg[1];

  if (rv<iv) {
    rv = iv;
    iv = Ravg[0];
    conj_flag = 1;
  }

  //   msg("rv = %d, iv = %d\n",rv,iv);
  //   msg("max_avg = %d, log2_approx = %d, shift = %d\n",avg[0], avg[1], output_shift);

  for (int k=0; k<6; k++) {
    (iv<(((int32_t)(alpha[a_idx]*rv))>>15)) ? (a_idx -= 32>>k) : (a_idx += 32>>k);
  }

  (conj_flag==1) ? (phase_idx = 127-(a_idx>>1)) : (phase_idx = (a_idx>>1));

  if (Ravg[1]<0)
    phase_idx = -phase_idx;

  return (phase_idx);
}
