#include "pt.h"
/* realistic sequence from kickoff (live config). returns 1 if the seat ends with MF on the ball */
static SimState s;
static void tr(const char*tag){ int me=s.seat_player[0]; printf("%s t=%u seat=%d own=%d ball(%.1f,%.1f) v=%.1f",tag,s.tick,me,s.ball.owner,tod(s.ball.pos.x),tod(s.ball.pos.y),spd(s.ball.vel));
  for(int i=1;i<5;i++) printf(" p%d(%.1f,%.1f|%.1f,%.1f)",i,tod(s.players[i].pos.x),tod(s.players[i].pos.y),tod(s.players[i].vel.x),tod(s.players[i].vel.y)); printf("\n"); }
int main(void){
  WpMatchConfig c=sim_default_config(); sim_init(&s,&c);
  tr("start");
  for(int t=0;t<30;t++) step1(&s,(WpSeatInput){127,0,0});
  tr("dribbled");
  WpSeatInput ps=stick_to(s.players[4].pos,s.players[3].pos,0,1);
  for(int t=0;t<3;t++){ WpSeatInput a=ps; a.buttons=WP_BTN_PASS; step1(&s,a);} 
  for(int t=0;t<200 && !(s.ball.owner==3);t++){ step1(&s,ps); if(t%10==0) tr("pass1"); }
  tr("MF has it");
  for(int t=0;t<120;t++){ step1(&s,(WpSeatInput){127,0,0}); if(t%20==0) tr("MF dribble"); }
  return 0; }
