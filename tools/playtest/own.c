#include "pt.h"
static SimState s;
int main(void){
  WpMatchConfig c=sim_default_config(); sim_init(&s,&c);
  for(int t=0;t<30;t++) step1(&s,(WpSeatInput){127,0,0});
  WpSeatInput ps=stick_to(s.players[4].pos,s.players[3].pos,0,1);
  printf("stick=(%d,%d) MF at (%.1f,%.1f) vel(%.1f,%.1f) ST(%.1f,%.1f)\n",ps.move_x,ps.move_y,tod(s.players[3].pos.x),tod(s.players[3].pos.y),tod(s.players[3].vel.x),tod(s.players[3].vel.y),tod(s.players[4].pos.x),tod(s.players[4].pos.y));
  for(int t=0;t<3;t++){ WpSeatInput a=ps; a.buttons=WP_BTN_PASS; step1(&s,a);} 
  for(int t=0;t<200;t++){ step1(&s,ps); if(s.events&(WP_EV_KICK|WP_EV_TOUCH|WP_EV_GOAL|WP_EV_OUT|WP_EV_BODY)) printf("t%u ev=%x owner=%d lt=%d seat=%d ball(%.2f,%.2f) v=%.1f kick.to=%d keeper(%.2f,%.2f) score %d-%d phase %d\n",s.tick,s.events,s.ball.owner,s.ball.last_touch,s.seat_player[0],tod(s.ball.pos.x),tod(s.ball.pos.y),spd(s.ball.vel),s.players[4].kick.to,tod(s.players[0].pos.x),tod(s.players[0].pos.y),s.score[0],s.score[1],s.phase); }
}
