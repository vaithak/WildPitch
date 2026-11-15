#include <stdlib.h>
/* goals.c - AI vs AI goal attribution: what kick scored, from where, own goals / keeper touches, defenders near the
 * shooter and goal-side at each shot, shots and goals by distance band. usage: goals [7|5] (12 x 3 min, no seats) */
#include <stdio.h>
#include <string.h>
#include "sim/sim.h"
static double tod(fx v){return (double)v/65536.0;}
int main(int argc,char**argv){
  int seven = argc>1 ? argv[1][0]=='7' : 1; int matches=12;
  static SimState s; double ndsum=0; long aheadsum=0, free3=0; long ownish=0,goals=0,shots=0,kinds[16]={0},kdist[4]={0},sdist[4]={0},gk_last=0,hdr=0;
  for(int m=0;m<matches;m++){
    WpMatchConfig cfg = seven? sim_config_7v7(): sim_config_5v5(); if (getenv("WP_STATS")) cfg.stats_mode = (uint8_t)atoi(getenv("WP_STATS")); sim_init(&s,&cfg);
    for(int k=0;k<WP_MAX_SEATS;k++) s.seat_player[k]=-1;
    s.ball.pos=fv2_make(FX_INT((m%5)-2),FX_INT((m%3)-1));
    WpInput in; memset(&in,0,sizeof in);
    int lk=-1,lkind=0; double ldist=0; fx L=s.pitch.length/2;
    for(long t=0;t<60L*60*3;t++){
      sim_step(&s,&in);
      if(s.events&WP_EV_KICK){ lk=s.ball.last_touch; lkind=s.players[lk].kick.kind;
        int tm=s.players[lk].team; fx gx = tm==0? L : -L; /* assume team0 attacks +x */
        double dx=tod(gx-s.ball.pos.x), dy=tod(s.ball.pos.y); double d=__builtin_sqrt(dx*dx+dy*dy);
        if (lkind==WP_KICK_SHOT){ shots++; { double nd=99; int ahead=0; for(int j=0;j<2*s.n_per_side;j++){ if(s.players[j].team==tm||s.players[j].role==WP_ROLE_KEEPER) continue; double ex=tod(s.players[j].pos.x-s.ball.pos.x), ey=tod(s.players[j].pos.y-s.ball.pos.y); double dd=__builtin_sqrt(ex*ex+ey*ey); if(dd<nd) nd=dd; if((tm==0?ex:-ex)>0 && dd<d) ahead++; } ndsum+=nd; aheadsum+=ahead; if(nd>3) free3++; } int b=d<10?0:d<16?1:d<22?2:3; sdist[b]++; }
        ldist=d; }
      if(s.events&WP_EV_GOAL){ goals++; int lt=s.ball.last_touch;
        int scorer_side = s.ball.pos.x>0 ? 0 : 1; /* ball in +x goal => team0 scored if team0 attacks +x */
        if(lk>=0 && s.players[lk].team!=scorer_side) ownish++;
        if(s.players[lt].role==WP_ROLE_KEEPER) gk_last++;
        if(lk>=0){ kinds[lkind]++; if(lkind==WP_KICK_SHOT){int b=ldist<10?0:ldist<16?1:ldist<22?2:3; kdist[b]++;} }
        if(s.events&WP_EV_HEADER) hdr++;
        printf("goal m%d t%ld side%d lastkick kind %d by p%d(team%d) dist %.1f lasttouch p%d%s\n",m,t,scorer_side,lkind,lk,lk>=0?s.players[lk].team:-1,ldist,lt,s.players[lt].role==WP_ROLE_KEEPER?" (GK)":"");
      }
    }
  }
  printf("goals %ld shots %ld | last kick kinds:",goals,shots); for(int k=0;k<16;k++) if(kinds[k]) printf(" k%d=%ld",k,kinds[k]);
  printf("\nlast-kick by other team (own goal/deflect) %ld, keeper last touch %ld\n",ownish,gk_last);
  printf("at shot: mean nearest defender %.1f m, defenders goal-side %.2f, shots with nobody within 3 m %ld\n",ndsum/(double)shots,(double)aheadsum/(double)shots,free3);
  printf("shots by dist <10/10-16/16-22/22+: %ld %ld %ld %ld ; goals from shots: %ld %ld %ld %ld\n",sdist[0],sdist[1],sdist[2],sdist[3],kdist[0],kdist[1],kdist[2],kdist[3]);
}
