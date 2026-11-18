/* corner_trace.c - corner header duels (D-053). Build: gcc -std=c17 -O2 -fwrapv -Isrc -Itests tools/playtest/corner_trace.c build/libwp_sim.a -o /tmp/corner_trace */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sim/sim.h"
#include "sim/sim_internal.h"
#include "sim/tuning.h"
static SimState s;
static double d(fx v){return v/65536.0;}
static void step(WpSeatInput in){WpInput a;memset(&a,0,sizeof a);a.seat[0]=in;sim_step(&s,&a);}
int main(int argc,char**argv){
  int hold=atoi(argv[1]), mode=atoi(argv[2]);
  WpMatchConfig c=sim_config_7v7(); c.seed=3; sim_init(&s,&c);
  s.ball.pos=fv2_make(s.pitch.length/2-FX(1.0),FX(8.0)); s.ball.vel=fv2_make(FX(5.0),0); s.ball.owner=-1; s.ball.last_touch=(int8_t)(s.n_per_side+2);
  WpSeatInput no={0,0,0};
  for(int t=0;t<1500 && !(s.restart_kind==WP_RS_CORNER && s.restart_taker>=0);t++) step(no);
  WpSeatInput in={0,-127,WP_BTN_LOB};
  for(int t=0;t<hold;t++) step(in);
  int me=s.seat_player[0]; in.buttons=0; step(in);
  for(int t=0;t<8;t++) step(in);
  int r=s.players[me].kick.to-1;
  printf("to p%d meet (%.1f,%.1f) t_meet? plan %d\n",r,d(s.pass_meet.x),d(s.pass_meet.y),s.pass_plan);
  for(int k=0;k<200;k++){ WpSeatInput q={0,0,0}; if(mode==1&&k>=18&&k<22) q.buttons=WP_BTN_SHOOT; 
    int lt=s.ball.last_touch; step(q);
    if(k%6==0||s.ball.last_touch!=lt){
      int seat=s.seat_player[0];
      int bd=-1; fx bdd=FX(99.0); for(int j=7;j<14;j++){fx dd=fv2_len(fv2_sub(s.players[j].pos,s.players[r].pos)); if(dd<bdd){bdd=dd;bd=j;}}
      printf(" k%3d ball (%.1f,%.1f,z%.2f) | rcv p%d (%.1f,%.1f) z%.2f act %d seat %d | marker p%d (%.1f,%.1f) z%.2f act %d | lt %d\n",k,d(s.ball.pos.x),d(s.ball.pos.y),d(s.ball.z),r,d(s.players[r].pos.x),d(s.players[r].pos.y),d(s.players[r].z),s.players[r].action,seat,bd,d(s.players[bd].pos.x),d(s.players[bd].pos.y),d(s.players[bd].z),s.players[bd].action,s.ball.last_touch);
      if(s.ball.last_touch!=lt && s.ball.last_touch!=me) break;
    }
  }
}
