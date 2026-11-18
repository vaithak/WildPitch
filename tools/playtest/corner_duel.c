/* corner_duel.c - corner header duels (D-053). Build: gcc -std=c17 -O2 -fwrapv -Isrc -Itests tools/playtest/corner_duel.c build/libwp_sim.a -o /tmp/corner_duel */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sim/sim.h"
#include "sim/sim_internal.h"
#include "sim/tuning.h"
#include "sim/contact_internal.h"
static SimState s;
static double d(fx v){return v/65536.0;}
static void step(WpSeatInput in){WpInput a;memset(&a,0,sizeof a);a.seat[0]=in;sim_step(&s,&a);}
int main(int argc,char**argv){
  int mode=atoi(argv[1]), skip=atoi(argv[2]), H0=atoi(argv[3]), H1=atoi(argv[4]), tr=argc>5;
  int att=0,deff=0,none=0,goals=0,n=0;
  for(int seed=0;seed<6;seed++) for(int hold=H0;hold<=H1;hold+=8){
    WpMatchConfig c=sim_config_7v7(); c.seed=(uint32_t)(seed+1); sim_init(&s,&c);
    s.ball.pos=fv2_make(s.pitch.length/2-FX(1.0),(seed&1)?-FX(8.0):FX(8.0)); s.ball.vel=fv2_make(FX(5.0),0); s.ball.owner=-1; s.ball.last_touch=(int8_t)(s.n_per_side+2);
    WpSeatInput no={0,0,0};
    int t; for(t=0;t<1500 && !(s.restart_kind==WP_RS_CORNER && s.restart_taker>=0);t++) step(no);
    fv2 ps=fv2_make(s.pitch.length/2-FX(9.0),0);
    fv2 u=fv2_norm(fv2_sub(ps,s.restart_spot));
    WpSeatInput in={(int8_t)(u.x*127/65536),(int8_t)(u.y*127/65536),0};
    if(skip){ step(no); WpSeatInput sk=in; sk.buttons=WP_BTN_PASS; step(sk); step(in); }
    for(;t<2000 && !(s.phase==WP_PHASE_PLAY && s.ball.owner==s.restart_taker);t++) step(in);
    int me=s.seat_player[0];
    for(int k=0;k<20;k++) step(in);
    in.buttons=WP_BTN_LOB; for(int k=0;k<hold;k++) step(in);
    in.buttons=0; step(in);
    int to=s.players[me].kick.to-1;
    int first=-1,sc0=s.score[0]; fv2 fp={0,0}; double fz=0;
    for(int k=0;k<240;k++){ WpSeatInput q={0,0,0}; if(mode==1&&k>=18&&k<22) q.buttons=WP_BTN_SHOOT; step(q);
      if(tr && k%10==0 && first<0){ AirChoice ac; int ok=to>=0?aerial_choose(&s,to,&ac):0; printf("   ac ok %d act %d head %d to (%.1f,%.1f) t %d z %.1f sp %.1f | ",ok,ok?ac.act:0,ok?ac.head:0,ok?d(ac.to.x):0,ok?d(ac.to.y):0,ok?ac.t:0,ok?d(ac.z):0,ok?d(ac.speed):0);} if(tr && k%10==0 && first<0) printf("   k%3d ball (%.1f,%.1f,%.1f) rcv (%.1f,%.1f) seat %d\n",k,d(s.ball.pos.x),d(s.ball.pos.y),d(s.ball.z),to>=0?d(s.players[to].pos.x):0,to>=0?d(s.players[to].pos.y):0,s.seat_player[0]);
      if(first<0 && s.ball.last_touch>=0 && s.ball.last_touch!=me){first=s.ball.last_touch; fp=s.players[first].pos; fz=d(s.ball.z);} }
    if(tr) printf("hold %d to p%d first p%d at (%.1f,%.1f) z %.1f\n",hold,to,first,d(fp.x),d(fp.y),fz);
    n++; if(first<0) none++; else if(s.players[first].team==0) att++; else deff++;
    goals+=s.score[0]>sc0;
  }
  printf("holds %d-%d mode %d skip %d: corners %d attackers %d defenders %d nobody %d goals %d\n",H0,H1,mode,skip,n,att,deff,none,goals);
}
