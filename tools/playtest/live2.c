#include "pt.h"
static SimState s;
static int park=1;
static void st(WpSeatInput si){ if(park) for(int i=5;i<10;i++){ s.players[i].pos=fv2_make(-FX(15)+FX_INT(i*2),FX(40)); s.players[i].vel=fv2_make(0,0);} step1(&s,si); }
static int setup_live(int dribble, uint16_t spr){
  WpMatchConfig c=sim_default_config(); sim_init(&s,&c);
  for(int t=0;t<30;t++) st((WpSeatInput){127,0,0});
  WpSeatInput ps=stick_to(s.players[4].pos,s.players[3].pos,0,1);
  for(int t=0;t<3;t++){ WpSeatInput a=ps; a.buttons=WP_BTN_PASS; st(a);} 
  for(int t=0;t<200 && !(s.ball.owner==3);t++) st(ps);
  for(int t=0;t<dribble;t++) st((WpSeatInput){127,0,spr});
  return s.ball.owner==3 && s.seat_player[0]==3;
}
static const char*PN[]={"keep-stick","release-stick","stick +x","keep+SPRINT","+x SPRINT","+x after 15t"};
int main(int argc,char**argv){
  int verbose=argc>1?atoi(argv[1]):-1; if(argc>2) park=0;
  int drib[]={40,90};
  for(int sp=0;sp<2;sp++)
  for(int di=0;di<2;di++)
  for(int pol=0;pol<6;pol++)
  for(int H=3; H<=39; H+=6){
    if(H==9) continue;
    if(!setup_live(drib[di], sp?WP_BTN_SPRINT:0)){ printf("setup fail\n"); continue; }
    uint16_t spr=(pol==3||pol==4||sp)?WP_BTN_SPRINT:0;
    WpPlayer*q=&s.players[4];
    WpSeatInput ps={0,0,0};
    for(int t=0;t<H;t++){ ps=stick_to(s.players[3].pos,q->pos,spr,1); WpSeatInput a=ps; a.buttons|=WP_BTN_PASS; st(a);} 
    int kt=-1; double v0=0,qv0=0; int got=-1,tg=-1,out=0; double minsp=99, back=0, vin=0, relin=0, qv_at=0, stillT=0, decelT=0, dist_run=0; double revdot=1;
    fv2 qdir0={0,0};
    for(int t=0;t<400;t++){
      WpSeatInput in=ps;
      if(kt>=0){ if(pol==1) in.move_x=in.move_y=0; if(pol==2||pol==4||(pol==5&&t-kt>=15)){in.move_x=127;in.move_y=0;} }
      st(in);
      if((s.events&WP_EV_KICK)&&kt<0&&s.ball.last_touch==3){ kt=t; v0=spd(s.ball.vel); qv0=spd(q->vel); qdir0=fv2_len(q->vel)>FX(0.5)?fv2_norm(q->vel):fv2_make(FX_ONE,0);}
      if(kt<0) continue;
      double sp2=spd(q->vel);
      if(s.ball.owner<0){ if(sp2<minsp)minsp=sp2; if(sp2<0.5)stillT++; vin=spd(s.ball.vel); relin=spd(fv2_sub(s.ball.vel,q->vel)); qv_at=sp2; dist_run+=sp2/60;
        double dt=sp2>0.3?tod(fv2_dot(fv2_norm(q->vel),qdir0)):1; if(dt<revdot) revdot=dt; }
      if(verbose==sp*1000+di*100+pol*10+H/6 && (t-kt)%5==0) printf("   t+%3d ball(%5.1f,%5.1f) v=%4.1f  st(%5.1f,%5.1f) v=(%4.1f,%4.1f) own=%d seat=%d pt=%d lt=%d\n",t-kt,tod(s.ball.pos.x),tod(s.ball.pos.y),spd(s.ball.vel),tod(q->pos.x),tod(q->pos.y),tod(q->vel.x),tod(q->vel.y),s.ball.owner,s.seat_player[0],s.pass_target,s.ball.last_touch);
      if(s.ball.owner>=0){got=s.ball.owner;tg=t-kt;break;}
      if(s.phase==WP_PHASE_OUT){out=1;break;}
      if(t-kt>300)break;
    }
    (void)decelT;
    printf("%s drib%2d %-13s H=%2d %s v0=%4.1f rcv_v@kick=%3.1f | got=%2d in %.2fs out=%d | ball@rcv %3.1f rel %3.1f rcv_v %3.1f | minV=%3.1f still=%.2fs  mostReversed(cos)=%+.2f ran=%.1fm\n",
      sp?"SPR":"jog",drib[di],PN[pol],H,H<=9?"tap ":"thru",v0,qv0,got,tg/60.0,out,vin,relin,qv_at,minsp,stillT/60,revdot,dist_run);
  }
}
