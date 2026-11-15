#include "pt.h"
#define ME 3
#define MATE 4
/* policy: dir code 0=keep pass stick,1=neutral,2=toward ball(live),3=away (pass dir),4=sideways +y,5=toward goal +x diag(+x,+y) ,6= +x */
static const char* PN[]={"keep","neutral","toward-ball","away(pass dir)","sideways","diag fwd","+x"};
static PR run(int stat, fv2 me, fv2 mate, int hold, int pol, int delay, uint16_t sprint, int verbose, fv2 mate_vel){
  static SimState s; PR r; memset(&r,0,sizeof r); r.recv_t=-1; r.who=-9; r.closest=99; r.took_over_t=-1;
  setup_empty(&s, ME); if(!stat) s.flags&=(uint8_t)~WP_SIMF_STATIC_OTHERS;
  s.seat_player[0]=ME; s.players[ME].pos=me; s.players[MATE].pos=mate; s.players[MATE].vel=mate_vel;
  s.ball.pos=fv2_add(me,fv2_make(FX(0.4),0)); for(int i=0;i<5;i++) step1(&s,(WpSeatInput){0,0,0});
  WpSeatInput ps=stick_to(s.players[ME].pos,s.players[MATE].pos,0,1);
  for(int t=0;t<hold;t++){ WpSeatInput a=ps; a.buttons=WP_BTN_PASS|sprint; step1(&s,a);} 
  int kt=-1; double px=0,py=0; int stopped_run=0;
  for(int t=0;t<400;t++){
    WpSeatInput in=ps; in.buttons=sprint;
    if(kt>=0 && t-kt>=delay){
      switch(pol){case 0: break; case 1: in.move_x=in.move_y=0; break;
        case 2: in=stick_to(s.players[MATE].pos,s.ball.pos,sprint,1); break;
        case 3: break; case 4: in.move_x=0; in.move_y=127; break; case 5: in.move_x=127; in.move_y=127; break; case 6: in.move_x=127;in.move_y=0;break;}
      if(pol==3){ fv2 dv=s.ball.vel; if(fv2_len(dv)>FX(0.3)) in=stick_to(fv2_make(0,0),dv,sprint,1); }
    }
    uint16_t ev=step1(&s,in);
    if((ev&WP_EV_KICK) && kt<0){ kt=t; r.kicked=1; r.v0=spd(s.ball.vel); px=tod(s.players[MATE].pos.x); py=tod(s.players[MATE].pos.y); r.recv_x0=px;r.recv_y0=py;}
    if(kt<0) continue;
    if(r.took_over_t<0 && s.pass_stick[0]==-128) r.took_over_t=t-kt;
    WpPlayer*q=&s.players[MATE];
    double d=bdist(&s,MATE); double rel=spd(fv2_sub(s.ball.vel,q->vel));
    if(s.ball.owner<0 && d<r.closest){ r.closest=d; r.rel_at_closest=rel; }
    r.run_dist+=spd(q->vel)/60.0;
    if(-tod(q->vel.x)>r.max_back) r.max_back=-tod(q->vel.x);
    if(spd(q->vel)<0.5 && s.ball.owner<0) r.stop_ticks++;
    if(verbose && (t-kt)%5==0) printf("  t+%3d ball(%5.1f,%5.1f) v=%4.1f  rcv(%5.1f,%5.1f) v=(%4.1f,%4.1f) d=%4.2f own=%d seat=%d tc=%d\n",t-kt,tod(s.ball.pos.x),tod(s.ball.pos.y),spd(s.ball.vel),tod(q->pos.x),tod(q->pos.y),tod(q->vel.x),tod(q->vel.y),d,s.ball.owner,s.seat_player[0],q->touch_cooldown);
    if(s.ball.owner>=0 && r.recv_t<0){ r.recv_t=t-kt; r.who=s.ball.owner; r.v_at_recv=spd(s.ball.vel); r.rv_at_recv=spd(q->vel); r.rel_at_recv=rel; break; }
    if(s.phase==WP_PHASE_OUT){ r.out=1; break; }
    if(spd(s.ball.vel)<0.05 && s.ball.z==0 && t-kt>30){ stopped_run++; if(stopped_run>60){ r.stopped=1; break;} }
  }
  (void)px;(void)py; return r;
}
int main(int argc,char**argv){
  int verbose=argc>1;
  for(int stat=1; stat>=1; stat--)
  for(int spr=0;spr<2;spr++)
  for(int pol=0;pol<7;pol++)
  for(int di=0;di<4;di++){
    int delays[]={0,12,30,50}; char tag[80];
    snprintf(tag,sizeof tag,"%s d%2d %s",PN[pol],delays[di],spr?"SPR":"jog");
    PR r=run(stat,fv2_make(-FX(10),0),fv2_make(FX(2),FX(0)),3,pol,delays[di],spr?WP_BTN_SPRINT:0,verbose&&pol==3&&di==0,fv2_make(0,0));
    pr_print(tag,&r);
  }
  return 0; }
