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

#include <string.h>
#include "PHY/defs_UE.h"
#include "filt96_32.h"
#include "T.h"
//#define DEBUG_CH
#include "PHY/LTE_UE_TRANSPORT/transport_proto_ue.h"
#include "PHY/LTE_REFSIG/lte_refsig.h"
#include "SCHED_UE/sched_UE.h"

int lte_dl_channel_estimation(PHY_VARS_UE *ue,
                              module_id_t eNB_id,
                              uint8_t eNB_offset,
                              unsigned char Ns,
                              unsigned char p,
                              unsigned char l,
                              unsigned char symbol) {
  int pilot[2][200] __attribute__((aligned(16)));
  unsigned char nu,aarx;
  unsigned short k;
  unsigned int rb,pilot_cnt;
  int16_t *f, *f2, *fl, *f2l2, *fr, *f2r2, *f2_dc, *f_dc;
  int ch_offset,symbol_offset;
  //  unsigned int n;
  //  int i;
  static int interpolateS11S12 = 1;
  uint16_t Nid_cell = (eNB_offset == 0) ? ue->frame_parms.Nid_cell : ue->measurements.adj_cell_id[eNB_offset-1];
  uint8_t nushift,pilot0,pilot1,pilot2,pilot3;
  uint8_t previous_thread_id = ue->current_thread_id[Ns>>1]==0 ? (RX_NB_TH-1):(ue->current_thread_id[Ns>>1]-1);
  LTE_UE_COMMON_PER_THREAD * vars=&ue->common_vars.common_vars_rx_data_per_thread[ue->current_thread_id[Ns>>1]];
  int **dl_ch_estimates         =vars->dl_ch_estimates[eNB_offset];
  int **dl_ch_estimates_previous=ue->common_vars.common_vars_rx_data_per_thread[previous_thread_id].dl_ch_estimates[eNB_offset];
  int **rxdataF=vars->rxdataF;
  pilot0 = 0;

  if (ue->frame_parms.Ncp == 0) {  // normal prefix
    pilot1 = 4;
    pilot2 = 7;
    pilot3 = 11;
  } else { // extended prefix
    pilot1 = 3;
    pilot2 = 6;
    pilot3 = 9;
  }

  // recompute nushift with eNB_offset corresponding to adjacent eNB on which to perform channel estimation
  nushift =  Nid_cell%6;

  if ((p==0) && (l==0) )
    nu = 0;
  else if ((p==0) && (l>0))
    nu = 3;
  else if ((p==1) && (l==0))
    nu = 3;
  else if ((p==1) && (l>0))
    nu = 0;
  else {
    LOG_E(PHY,"lte_dl_channel_estimation: p %d, l %d -> ERROR\n",p,l);
    return(-1);
  }

  //ch_offset     = (l*(ue->frame_parms.ofdm_symbol_size));
  if (ue->high_speed_flag == 0) // use second channel estimate position for temporary storage
    ch_offset     = ue->frame_parms.ofdm_symbol_size ;
  else
    ch_offset     = ue->frame_parms.ofdm_symbol_size*symbol;

  symbol_offset = ue->frame_parms.ofdm_symbol_size*symbol;
  k = (nu + nushift)%6;
#ifdef DEBUG_CH
  printf("Channel Estimation : ThreadId %d, eNB_offset %d cell_id %d ch_offset %d, OFDM size %d, Ncp=%d, l=%d, Ns=%d, k=%d\n",ue->current_thread_id[Ns>>1], eNB_offset,Nid_cell,ch_offset,
         ue->frame_parms.ofdm_symbol_size,
         ue->frame_parms.Ncp,l,Ns,k);
#endif

  switch (k) {
    case 0 :
      f=filt24_0;  //for first pilot of RB, first half
      f2=filt24_2; //for second pilot of RB, first half
      fl=filt24_0; //for first pilot of leftmost RB
      f2l2=filt24_2;
      //    fr=filt24_2r; //for first pilot of rightmost RB
      fr=filt24_0r2; //for first pilot of rightmost RB
      //    f2r2=filt24_0r2;
      f2r2=filt24_2r;
      f_dc=filt24_0_dcr;
      f2_dc=filt24_2_dcl;
      break;

    case 1 :
      f=filt24_1;
      f2=filt24_3;
      fl=filt24_1l;
      f2l2=filt24_3l2;
      fr=filt24_1r2;
      f2r2=filt24_3r;
      f_dc=filt24_1_dcr;  //for first pilot of RB, first half
      f2_dc=filt24_3_dcl;  //for first pilot of RB, first half
      break;

    case 2 :
      f=filt24_2;
      f2=filt24_4;
      fl=filt24_2l;
      f2l2=filt24_4l2;
      fr=filt24_2r2;
      f2r2=filt24_4r;
      f_dc=filt24_2_dcr;  //for first pilot of RB, first half
      f2_dc=filt24_4_dcl;  //for first pilot of RB, first half
      break;

    case 3 :
      f=filt24_3;
      f2=filt24_5;
      fl=filt24_3l;
      f2l2=filt24_5l2;
      fr=filt24_3r2;
      f2r2=filt24_5r;
      f_dc=filt24_3_dcr;  //for first pilot of RB, first half
      f2_dc=filt24_5_dcl;  //for first pilot of RB, first half
      break;

    case 4 :
      f=filt24_4;
      f2=filt24_6;
      fl=filt24_4l;
      f2l2=filt24_6l2;
      fr=filt24_4r2;
      f2r2=filt24_6r;
      f_dc=filt24_4_dcr;  //for first pilot of RB, first half
      f2_dc=filt24_6_dcl;  //for first pilot of RB, first half
      break;

    case 5 :
      f=filt24_5;
      f2=filt24_7;
      fl=filt24_5l;
      f2l2=filt24_7l2;
      fr=filt24_5r2;
      f2r2=filt24_7r;
      f_dc=filt24_5_dcr;  //for first pilot of RB, first half
      f2_dc=filt24_7_dcl;  //for first pilot of RB, first half
      break;

    default:
      LOG_E(PHY,"lte_dl_channel_estimation: k=%d -> ERROR\n",k);
      return(-1);
      break;
  }

  // generate pilot
  lte_dl_cell_spec_rx(ue,
                      eNB_offset,
                      &pilot[p][0],
                      Ns,
                      (l==0)?0:1,
                      p);

  for (aarx=0; aarx<ue->frame_parms.nb_antennas_rx; aarx++) {
    c16_t *pil = (c16_t *)&pilot[p][0];
    c16_t *rxF = (c16_t *)&rxdataF[aarx][symbol_offset + k + ue->frame_parms.first_carrier_offset];
    c16_t *dl_ch = (c16_t *)&dl_ch_estimates[(p << 1) + aarx][ch_offset];
    //    if (eNb_id==0)
    memset(dl_ch, 0, sizeof(c16_t) * ue->frame_parms.ofdm_symbol_size);

    if (ue->high_speed_flag==0) // multiply previous channel estimate by ch_est_alpha
      mult_complex_vector_real_scalar(dl_ch - ue->frame_parms.ofdm_symbol_size,
                                      ue->ch_est_alpha,
                                      dl_ch - ue->frame_parms.ofdm_symbol_size,
                                      ue->frame_parms.ofdm_symbol_size);

#ifdef DEBUG_CH
    printf("k %d, first_carrier %d\n",k,ue->frame_parms.first_carrier_offset);
#endif

    if ((ue->frame_parms.N_RB_DL==6)  ||
        (ue->frame_parms.N_RB_DL==50) ||
        (ue->frame_parms.N_RB_DL==100)) {
      // First half of pilots
      //  Treat first 2 pilots specially (left edge)
      multadd_real_vector_complex_scalar(fl, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
      rxF += 6;
      dl_ch += 4;
      multadd_real_vector_complex_scalar(f2l2, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
      rxF += 6;
      dl_ch += 8;

      for (pilot_cnt = 2; pilot_cnt < ((ue->frame_parms.N_RB_DL) - 1); pilot_cnt += 2) {
        multadd_real_vector_complex_scalar(f, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 4;
        multadd_real_vector_complex_scalar(f2, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 8;
      }
      // Second half of RBs
      k = (nu + nushift)%6;
      rxF = (c16_t *)&rxdataF[aarx][symbol_offset + 1 + k];
      for (pilot_cnt = 0; pilot_cnt < ((ue->frame_parms.N_RB_DL) - 3); pilot_cnt += 2) {
        multadd_real_vector_complex_scalar(f, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 4;
        multadd_real_vector_complex_scalar(f2, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 8;
      }
      multadd_real_vector_complex_scalar(fr, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
      rxF += 6;
      dl_ch += 4;
      multadd_real_vector_complex_scalar(f2r2, c16mulShift(*pil, rxF[0], 15), dl_ch, 24);
    } else if (ue->frame_parms.N_RB_DL == 25) {
      multadd_real_vector_complex_scalar(fl, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
      rxF += 6;
      dl_ch += 4;
      multadd_real_vector_complex_scalar(f2l2, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
      rxF += 6;
      dl_ch += 8;

      for (pilot_cnt = 2; pilot_cnt < 24; pilot_cnt += 2) {
        multadd_real_vector_complex_scalar(f, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 4;
        multadd_real_vector_complex_scalar(f2, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 8;
      }
      multadd_real_vector_complex_scalar(f_dc, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
      dl_ch += 4;
      // Second half of RBs
      rxF = (c16_t *)&rxdataF[aarx][symbol_offset + 1 + k];
      multadd_real_vector_complex_scalar(f2_dc, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
      rxF += 6;
      dl_ch += 8;
      for (pilot_cnt = 0; pilot_cnt < 22; pilot_cnt += 2) {
        multadd_real_vector_complex_scalar(f, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 4;
        multadd_real_vector_complex_scalar(f2, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 8;
      }
      multadd_real_vector_complex_scalar(fr, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
      rxF += 6;
      dl_ch += 4;
      multadd_real_vector_complex_scalar(f2r2, c16mulShift(*pil, rxF[0], 15), dl_ch, 24);
    } else if (ue->frame_parms.N_RB_DL == 15) {
      for (rb = 0; rb < 28; rb += 4) {
        multadd_real_vector_complex_scalar(f, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 4;
        multadd_real_vector_complex_scalar(f2, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 8;
      }
      multadd_real_vector_complex_scalar(f, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
      dl_ch += 4;
      //Second half of RBs
      rxF = (c16_t *)&rxdataF[aarx][((symbol_offset + 1 + nushift + (3 * p)))];
      multadd_real_vector_complex_scalar(f2, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
      rxF += 6;
      dl_ch += 8;

      for (rb = 0; rb < 28; rb += 4) {
        multadd_real_vector_complex_scalar(f, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 4;
        multadd_real_vector_complex_scalar(f2, c16mulShift(*pil++, rxF[0], 15), dl_ch, 24);
        rxF += 6;
        dl_ch += 8;
      }
    } else {
      LOG_E(PHY,"channel estimation not implemented for ue->frame_parms.N_RB_DL = %d\n",ue->frame_parms.N_RB_DL);
    }
    int symb_sz = ue->frame_parms.ofdm_symbol_size;
    if (ue->perfect_ce == 0) {
      // Temporal Interpolation
      // printf("ch_offset %d\n",ch_offset);
      dl_ch = (c16_t *)&dl_ch_estimates[(p << 1) + aarx][ch_offset];

      if (ue->high_speed_flag == 0) {
        multadd_complex_vector_real_scalar(dl_ch, 32767 - ue->ch_est_alpha, dl_ch - symb_sz, symb_sz);
      } else { // high_speed_flag == 1
        if (symbol == 0) {
          //      printf("Interpolating %d->0\n",4-ue->frame_parms.Ncp);
          //      dl_ch_prev = (int16_t *)&dl_ch_estimates[(p<<1)+aarx][(4-ue->frame_parms.Ncp)*(symb_sz)];
          if(((Ns>>1)!=0) || ( ((Ns>>1)==0) && interpolateS11S12)) {
            //LOG_I(PHY,"Interpolate s11-->s0 to get s12 and s13  Ns %d \n", Ns);
            c16_t *dl_ch_prev = (c16_t *)&dl_ch_estimates_previous[(p << 1) + aarx][pilot3 * (symb_sz)];
            mult_complex_vector_real_scalar(dl_ch_prev, 21845, dl_ch_prev + symb_sz, symb_sz);
            multadd_complex_vector_real_scalar(dl_ch, 10923, dl_ch_prev + symb_sz, symb_sz);
            mult_complex_vector_real_scalar(dl_ch_prev, 10923, dl_ch_prev + 2 * symb_sz, symb_sz);
            multadd_complex_vector_real_scalar(dl_ch, 21845, dl_ch_prev + 2 * symb_sz, symb_sz);
          }

          interpolateS11S12 = 1;
        } // this is 1/3,2/3 combination for pilots spaced by 3 symbols
        else if (symbol == pilot1) {
          c16_t *dl_ch_prev = (c16_t *)&dl_ch_estimates[(p << 1) + aarx][0];

          //LOG_I(PHY,"Interpolate s0-->s4 to get s1 s2 and s3 Ns %d \n", Ns);
          if (ue->frame_parms.Ncp==0) {// pilot spacing 4 symbols (1/4,1/2,3/4 combination)
            uint8_t previous_subframe;

            if(Ns>>1 == 0)
              previous_subframe = 9;
            else
              previous_subframe = ((Ns>>1) - 1 )%9;

            if((subframe_select(&ue->frame_parms,previous_subframe) == SF_UL)) {
              mult_complex_vector_real_scalar(dl_ch_prev, 328, dl_ch_prev + symb_sz, symb_sz);
              multadd_complex_vector_real_scalar(dl_ch, 32440, dl_ch_prev + symb_sz, symb_sz);
              mult_complex_vector_real_scalar(dl_ch_prev, 328, dl_ch_prev + 2 * symb_sz, symb_sz);
              multadd_complex_vector_real_scalar(dl_ch, 32440, dl_ch_prev + 2 * symb_sz, symb_sz);
              mult_complex_vector_real_scalar(dl_ch_prev, 8192, dl_ch_prev + 3 * symb_sz, symb_sz);
              multadd_complex_vector_real_scalar(dl_ch, 32440, dl_ch_prev + 3 * symb_sz, symb_sz);
            } else {
              mult_complex_vector_real_scalar(dl_ch_prev, 24576, dl_ch_prev + symb_sz, symb_sz);
              multadd_complex_vector_real_scalar(dl_ch, 8192, dl_ch_prev + symb_sz, symb_sz);
              mult_complex_vector_real_scalar(dl_ch_prev, 16384, dl_ch_prev + 2 * symb_sz, symb_sz);
              multadd_complex_vector_real_scalar(dl_ch, 16384, dl_ch_prev + 2 * symb_sz, symb_sz);
              mult_complex_vector_real_scalar(dl_ch_prev, 8192, dl_ch_prev + 3 * symb_sz, symb_sz);
              multadd_complex_vector_real_scalar(dl_ch, 24576, dl_ch_prev + 3 * symb_sz, symb_sz);
            }
          } else {
            mult_complex_vector_real_scalar(dl_ch_prev, 328, dl_ch_prev + symb_sz, symb_sz);
            multadd_complex_vector_real_scalar(dl_ch, 21845, dl_ch_prev + symb_sz, symb_sz);
            mult_complex_vector_real_scalar(dl_ch_prev, 21845, dl_ch_prev + 2 * symb_sz, symb_sz);
            multadd_complex_vector_real_scalar(dl_ch, 10923, dl_ch_prev + 2 * symb_sz, symb_sz);
          } // pilot spacing 3 symbols (1/3,2/3 combination)
        } else if (symbol == pilot2) {
          c16_t *dl_ch_prev = (c16_t *)&dl_ch_estimates[(p << 1) + aarx][pilot1 * symb_sz];
          mult_complex_vector_real_scalar(dl_ch_prev, 21845, dl_ch_prev + symb_sz, symb_sz);
          multadd_complex_vector_real_scalar(dl_ch, 10923, dl_ch_prev + symb_sz, symb_sz);
          mult_complex_vector_real_scalar(dl_ch_prev, 10923, dl_ch_prev + 2 * symb_sz, symb_sz);
          multadd_complex_vector_real_scalar(dl_ch, 21845, dl_ch_prev + 2 * symb_sz, symb_sz);
        } else { // symbol == pilot3
          //      printf("Interpolating 0->%d\n",4-ue->frame_parms.Ncp);
          c16_t *dl_ch_prev = (c16_t *)&dl_ch_estimates[(p << 1) + aarx][pilot2 * symb_sz];

          if (ue->frame_parms.Ncp==0) {// pilot spacing 4 symbols (1/4,1/2,3/4 combination)
            mult_complex_vector_real_scalar(dl_ch_prev, 24576, dl_ch_prev + symb_sz, symb_sz);
            multadd_complex_vector_real_scalar(dl_ch, 8192, dl_ch_prev + symb_sz, symb_sz);
            mult_complex_vector_real_scalar(dl_ch_prev, 16384, dl_ch_prev + 2 * symb_sz, symb_sz);
            multadd_complex_vector_real_scalar(dl_ch, 16384, dl_ch_prev + 2 * symb_sz, symb_sz);
            mult_complex_vector_real_scalar(dl_ch_prev, 8192, dl_ch_prev + 3 * symb_sz, symb_sz);
            multadd_complex_vector_real_scalar(dl_ch, 24576, dl_ch_prev + 3 * symb_sz, symb_sz);
          } else {
            mult_complex_vector_real_scalar(dl_ch_prev, 10923, dl_ch_prev + symb_sz, symb_sz);
            multadd_complex_vector_real_scalar(dl_ch, 21845, dl_ch_prev + symb_sz, symb_sz);
            mult_complex_vector_real_scalar(dl_ch_prev, 21845, dl_ch_prev + 2 * symb_sz, symb_sz);
            multadd_complex_vector_real_scalar(dl_ch, 10923, dl_ch_prev + 2 * symb_sz, symb_sz);
          } // pilot spacing 3 symbols (1/3,2/3 combination)

          if((ue->rx_offset_diff !=0) && ((Ns>>1) == 9)) {
            //LOG_I(PHY,"Extrapolate s7-->s11 to get s12 and s13 Ns %d\n", Ns);
            interpolateS11S12 = 0;
            //LOG_E(PHY,"Interpolate s7--s11 s12 s13 pilot 3 Ns %d l %d symbol %d \n", Ns, l, symbol);
            int16_t *dlChEst_ofdm11 = (int16_t *)&dl_ch_estimates[(p << 1) + aarx][pilot3 * symb_sz];
            int16_t *dlChEst_ofdm7 = (int16_t *)&dl_ch_estimates[(p << 1) + aarx][pilot2 * symb_sz];
            // interpolate ofdm s12: 5/4*ofdms11 + -1/4*ofdms7 5/4 q1.15 40960 -1/4 q1.15 8192
            int16_t *dlChEst_ofdm12 = (int16_t *)&dl_ch_estimates[(p << 1) + aarx][12 * symb_sz];

            for (int i = 0; i < (2 * symb_sz); i++) {
              int64_t tmp_mult = 0;
              tmp_mult = ((int64_t)dlChEst_ofdm11[i] * 40960 - (int64_t)dlChEst_ofdm7[i] * 8192);
              tmp_mult = tmp_mult >> 15;
              dlChEst_ofdm12[i] = tmp_mult;
            }

            // interpolate ofdm s13: 3/2*ofdms11 + -1/2*ofdms7 3/2 q1.15 49152 1/2 q1.15 16384
            int16_t *dlChEst_ofdm13 = (int16_t *)&dl_ch_estimates[(p << 1) + aarx][13 * symb_sz];

            for (int i = 0; i < (2 * symb_sz); i++) {
              int64_t tmp_mult = 0;
              tmp_mult = ((int64_t)dlChEst_ofdm11[i] * 49152 - (int64_t)dlChEst_ofdm7[i] * 16384);
              tmp_mult = tmp_mult >> 15;
              dlChEst_ofdm13[i] = tmp_mult;
            }
          }
        }
      }
    }
  }

  int s = ue->frame_parms.ofdm_symbol_size;
  if (s != 128 && s != 256 && s != 512 && s != 1024 && s != 1536 && s != 2048)
    s = 512;

  if( ((Ns%2) == 0) && (l == pilot0)) {
    // do ifft of channel estimate
    for (aarx=0; aarx<ue->frame_parms.nb_antennas_rx; aarx++)
      for (p=0; p<ue->frame_parms.nb_antenna_ports_eNB; p++) {
        if (dl_ch_estimates[(p<<1)+aarx]) {
          //LOG_I(PHY,"Channel Impulse Computation Slot %d ThreadId %d Symbol %d \n", Ns, ue->current_thread_id[Ns>>1], l);
          idft(get_idft(s),
               (int16_t *)&dl_ch_estimates[(p << 1) + aarx][8],
               (int16_t *)vars->dl_ch_estimates_time[eNB_offset][(p << 1) + aarx],
               1);
        }
      }
  }

  T(T_UE_PHY_DL_CHANNEL_ESTIMATE, T_INT(eNB_id), T_INT(0),
    T_INT(ue->proc.proc_rxtx[ue->current_thread_id[Ns>>1]].frame_rx%1024), T_INT(ue->proc.proc_rxtx[ue->current_thread_id[Ns>>1]].subframe_rx),
    T_INT(0), T_BUFFER(&vars->dl_ch_estimates_time[eNB_offset][0][0], 512  * 4));
  return(0);
}

