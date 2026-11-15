#include "pt.h"
static SimState s;
static void st(WpSeatInput si){ for(int i=5;i<10;i++){ s.players[i].pos=fv2_make(-FX(15)+FX_INT(i*2),FX(40)); s.players[i].vel=fv2_make(0,0);} step1(&s,si); }
int main(void){
  WpMatchConfig c=sim_default_config(); sim_init(&s,&c);
  s.seat_player[0]=3; s.ball.pos=fv2_add(s.players[3].pos,fv2_make(FX(0.4),0)); for(int t=0;t<60&&s.ball.owner!=3;t++) st((WpSeatInput){0,0,0});
  WpSeatInput ps;
  for(int t=0;t<40;t++) st((WpSeatInput){127,0,0});
  printf("MF on ball at (%.1f,%.1f), ST at (%.1f,%.1f)\n",tod(s.players[3].pos.x),tod(s.players[3].pos.y),tod(s.players[4].pos.x),tod(s.players[4].pos.y));
  /* 1: MF taps to ST (stick toward ST) */
  ps=stick_to(s.players[3].pos,s.players[4].pos,0,1);
  for(int t=0;t<3;t++){ WpSeatInput a=ps; a.buttons=WP_BTN_PASS; st(a);} 
  int t0=0; for(int t=0;t<200 && !(s.ball.owner==4);t++){ st(ps); t0=t; }
  printf("ST got it after %d ticks; seat=%d. MF (now AI) at (%.1f,%.1f) vel (%.1f,%.1f)\n",t0,s.seat_player[0],tod(s.players[3].pos.x),tod(s.players[3].pos.y),tod(s.players[3].vel.x),tod(s.players[3].vel.y));
  for(int t=0;t<60;t++){ st((WpSeatInput){0,0,0}); if(t%15==14) printf("  +%d: ball(%.1f,%.1f) MF(%.1f,%.1f) v(%.1f,%.1f)  MF is %.1f m %s the ball\n",t+1,tod(s.ball.pos.x),tod(s.ball.pos.y),tod(s.players[3].pos.x),tod(s.players[3].pos.y),tod(s.players[3].vel.x),tod(s.players[3].vel.y),fabs(tod(s.players[3].pos.x-s.ball.pos.x)), s.players[3].pos.x>s.ball.pos.x?"AHEAD of":"BEHIND"); }
  /* try a through ball 'return' toward MF with stick +x (forward): who gets picked? */
  WpSeatInput f={127,0,WP_BTN_PASS}; for(int t=0;t<24;t++) st(f);
  st((WpSeatInput){127,0,0});
  for(int t=0;t<6;t++) st((WpSeatInput){127,0,0});
  printf("Through ball with stick +x from ST: kick.to=%d (0=nobody in cone -> 12 m/s into space), ball v=%.1f dir=(%.2f,%.2f)\n",s.players[4].kick.to,spd(s.ball.vel),tod(fv2_norm(s.ball.vel).x),tod(fv2_norm(s.ball.vel).y));
}
