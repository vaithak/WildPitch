#include "pt.h"
static SimState s;
int main(void){
  for(int rel=-1; rel<=30; rel+= (rel<0?1:5)){
  WpMatchConfig c=sim_default_config(); sim_init(&s,&c);
  for(int t=0;t<30;t++) step1(&s,(WpSeatInput){127,0,0});
  WpSeatInput ps=stick_to(s.players[4].pos,s.players[3].pos,0,1);
  for(int t=0;t<3;t++){ WpSeatInput a=ps; a.buttons=WP_BTN_PASS; step1(&s,a);} 
  int recv=-1; double vin=0;
  for(int t=0;t<250;t++){ WpSeatInput in=ps; if(recv>=0 && t-recv>=rel) in=(WpSeatInput){0,0,0}; if(rel<0) in=(WpSeatInput){0,0,0};
    if(s.ball.owner<0) vin=spd(s.ball.vel);
    step1(&s,in); if(s.ball.owner==0 && recv<0) {recv=t; printf("  keeper receives ball arriving at %.1f m/s at x=%.2f; ",vin,tod(s.ball.pos.x));}
    if(s.phase==WP_PHASE_GOAL){ printf("stick released %d ticks after reception -> OWN GOAL\n",rel); break;} }
  if(s.phase!=WP_PHASE_GOAL) printf("stick released %d ticks after reception -> no goal, ball at x=%.1f owner=%d\n",rel,tod(s.ball.pos.x),s.ball.owner);
  }
}
