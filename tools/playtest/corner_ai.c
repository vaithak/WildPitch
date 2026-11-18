/* corner_ai.c - corner header duels (D-053). Build: gcc -std=c17 -O2 -fwrapv -Isrc -Itests tools/playtest/corner_ai.c build/libwp_sim.a -o /tmp/corner_ai */
#include <stdio.h>
#include <string.h>
#include "sim/sim.h"
#include "sim/sim_internal.h"
#include "sim/tuning.h"
static SimState s;
static double d(fx v){return v/65536.0;}
int main(void){
  int att=0,deff=0,none=0,goals=0,n=0;
  for(int seed=0;seed<30;seed++){
    WpMatchConfig c=sim_config_7v7(); c.seed=(uint32_t)(seed+1); sim_init(&s,&c);
    for(int k=0;k<WP_MAX_SEATS;k++) s.seat_player[k]=-1;
    s.ball.pos=fv2_make(s.pitch.length/2-FX(1.0),(seed&1)?-FX(8.0):FX(8.0)); s.ball.vel=fv2_make(FX(5.0),0); s.ball.owner=-1; s.ball.last_touch=(int8_t)(s.n_per_side+2);
    WpInput in; memset(&in,0,sizeof in);
    int t; for(t=0;t<2000 && !(s.restart_kind==WP_RS_CORNER && s.restart_taker>=0);t++) sim_step(&s,&in);
    int tk=s.restart_taker;
    for(;t<3000 && s.restart_kind==WP_RS_CORNER;t++) sim_step(&s,&in);
    if(seed<3){ printf("seed %d kicked at %d; positions at kick:\n",seed,t); for(int j=0;j<14;j++) printf("  p%d t%d (%.1f,%.1f)\n",j,s.players[j].team,d(s.players[j].pos.x),d(s.players[j].pos.y)); }
    int first=-1,sc0=s.score[0];
    for(int k=0;k<240;k++){ sim_step(&s,&in); if(first<0 && s.ball.last_touch>=0 && s.ball.last_touch!=tk) first=s.ball.last_touch; }
    n++; if(first<0) none++; else if(s.players[first].team==0) att++; else deff++;
    goals+=s.score[0]>sc0;
  }
  printf("AI corners %d: first touch attackers %d defenders %d nobody %d goals %d\n",n,att,deff,none,goals);
}
