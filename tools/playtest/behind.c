#include "pt.h"
/* passer directly behind a runner on the same line: does the ball roll through his body? */
static SimState s;
static void park(void){ for(int i=5;i<10;i++){ s.players[i].pos=fv2_make(-FX(15)+FX_INT(i*2),FX(40)); s.players[i].vel=fv2_make(0,0);} }
int main(void){
  for(int gap=4; gap<=12; gap+=4) for(int H=3; H<=36; H+=11) for(int spr=0;spr<2;spr++) for(int pol=0;pol<2;pol++){
    WpMatchConfig c=sim_default_config(); sim_init(&s,&c);
    s.seat_player[0]=3; s.seat_player[1]=4;
    s.players[3].pos=fv2_make(-FX(14),0); s.players[4].pos=fv2_make(-FX(14)+FX_INT(gap),0);
    s.players[1].pos=fv2_make(-FX(14),-FX(8)); s.players[2].pos=fv2_make(-FX(14),FX(8));
    s.ball.pos=fv2_make(-FX(13.6),0);
    WpInput in; memset(&in,0,sizeof in);
    for(int t=0;t<10;t++){ park(); sim_step(&s,&in);} 
    in.seat[1]=(WpSeatInput){127,0,(uint16_t)(spr?WP_BTN_SPRINT:0)}; in.seat[0]=(WpSeatInput){127,0,0};
    for(int t=0;t<30;t++){ park(); sim_step(&s,&in);} 
    for(int t=0;t<H;t++){ in.seat[0]=(WpSeatInput){127,0,WP_BTN_PASS}; park(); sim_step(&s,&in);} 
    in.seat[0]=(WpSeatInput){127,0,(uint16_t)(spr?WP_BTN_SPRINT:0)};
    s.seat_player[1]=-1; memset(&in.seat[1],0,sizeof in.seat[1]);
    if(pol==1) in.seat[0].move_x=0;
    int kt=-1; double mind=99, rel_at=0, vb=0, rv=0; int got=-1, tg=-1, through_body=0, bodyhit=0; double prev_along=-99;
    for(int t=0;t<300;t++){ park(); sim_step(&s,&in);
      if((s.events&WP_EV_KICK)&&kt<0) kt=t;
      if(kt<0) continue;
      WpPlayer*q=&s.players[4];
      if(s.events&WP_EV_BODY) bodyhit++;
      fv2 d=fv2_sub(s.ball.pos,q->pos); double dd=tod(fv2_len(d));
      double along=tod(d.x);
      if(s.ball.owner<0){ if(dd<mind){mind=dd; rel_at=spd(fv2_sub(s.ball.vel,q->vel));}
        if(prev_along<0 && along>=0 && fabs(tod(d.y))<0.35) through_body=1; prev_along=along; vb=spd(s.ball.vel); rv=spd(q->vel);}
      if(s.ball.owner>=0){got=s.ball.owner; tg=t-kt; break;}
      if(s.phase!=WP_PHASE_PLAY) break;
    }
    printf("gap %2d m %s %s H=%2d: got=%d t=%.2fs closest-to-centre %.2f m (rel %.1f) ball passed THROUGH his body=%d bodyhits=%d ball@rcv %.1f rcv %.1f phase=%d\n",gap,spr?"SPR":"jog",pol?"stick-released":"stick+x",H,got,tg/60.0,mind,rel_at,through_body,bodyhit,vb,rv,s.phase);
  }
}
