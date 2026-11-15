#include "pt.h"
#define ME 3
#define MATE 4
static SimState s;
/* mode: 0 nudge (press dir for 12 ticks from start, then release); 1 hold dir from start until end; 2 press dir starting 12 ticks before arrival, keep; 3 steer at ball (8-way) from start
   dir: angle in 45deg steps; we skip dirs equal to the pass stick */
typedef struct{int got; double closest; int out; int other;} R;
static R run(fv2 mate, int mode, int dir, int start, uint16_t spr, int hold){
  R r={0,99,0,0};
  setup_empty(&s, ME); s.seat_player[0]=ME; fv2 me=fv2_make(-FX(10),0); s.players[ME].pos=me; s.players[MATE].pos=fv2_add(me,mate);
  s.ball.pos=fv2_add(me,fv2_make(FX(0.4),0)); for(int i=0;i<5;i++) step1(&s,(WpSeatInput){0,0,0});
  WpSeatInput ps=stick_to(s.players[ME].pos,s.players[MATE].pos,0,1);
  for(int t=0;t<hold;t++){ WpSeatInput a=ps; a.buttons=WP_BTN_PASS|spr; step1(&s,a);} 
  int kt=-1;
  double a=dir*M_PI/4; int8_t dx=(int8_t)(fabs(cos(a))<0.1?0:(cos(a)>0?127:-127)), dy=(int8_t)(fabs(sin(a))<0.1?0:(sin(a)>0?127:-127));
  for(int t=0;t<400;t++){
    WpSeatInput in=ps; in.buttons=spr;
    if(kt>=0){ int k=t-kt;
      if(mode==0 && k>=start && k<start+12){in.move_x=dx;in.move_y=dy;}
      if(mode==0 && k>=start+12){in.move_x=0;in.move_y=0;}
      if(mode==1){in.move_x=dx;in.move_y=dy;}
      if(mode==2 && k>=start){in.move_x=dx;in.move_y=dy;}
      if(mode==3 && k>=start){ in=stick_to(s.players[MATE].pos,s.ball.pos,spr,1);} }
    step1(&s,in);
    if((s.events&WP_EV_KICK)&&kt<0)kt=t;
    if(kt<0)continue;
    double d=bdist(&s,MATE); if(s.ball.owner<0&&d<r.closest)r.closest=d;
    if(s.ball.owner>=0){ r.got=s.ball.owner==MATE; r.other=!r.got; break;}
    if(s.phase==WP_PHASE_OUT){r.out=1;break;}
    if(t-kt>300)break;
  }
  return r;
}
int main(int argc,char**argv){
  int hold=argc>1?atoi(argv[1]):3;
  fv2 mates[]={{FX(12),0},{FX(10),FX(6)},{FX(5),FX(10)},{FX(12),-FX(5)},{FX(20),FX(3)},{FX(7),FX(2)}};
  const char*MN[]={"nudge 12t from hand-off","nudge 12t mid-flight","hold dir from hand-off","press 12t before arrival (keep)","steer at ball from hand-off"};
  /* categories relative to ball travel direction: toward ball (cos<-0.5), sideways (|cos|<=0.5), with ball/forward (cos>0.5) */
  for(int sp=0;sp<2;sp++){
   printf("\n=== %s, %s ===\n", sp?"SPRINT held":"jog", hold>9?"THROUGH BALL":"tap pass");
   for(int m=0;m<5;m++){
    int n[3]={0,0,0}, f[3]={0,0,0};
    for(int mi=0;mi<6;mi++){
      fv2 pstick; WpSeatInput ps=stick_to(fv2_make(0,0),mates[mi],0,1); (void)pstick;
      R base=run(mates[mi],1,0,0,sp?WP_BTN_SPRINT:0,hold); (void)base;
      /* baseline arrival: keep stick */
      int arr=60; { setup_empty(&s,ME); }
      for(int d=0;d<8;d++){
        double a=d*M_PI/4; int8_t dx=(int8_t)(fabs(cos(a))<0.1?0:(cos(a)>0?127:-127)), dy=(int8_t)(fabs(sin(a))<0.1?0:(sin(a)>0?127:-127));
        if(dx==ps.move_x&&dy==ps.move_y) continue;   /* same as pass stick = no takeover */
        if(m==4 && d>0) continue;
        double bx=tod(mates[mi].x), by=tod(mates[mi].y), bl=sqrt(bx*bx+by*by); double c=(cos(a)*bx+sin(a)*by)/bl;
        int cat= m==4?0: (c<-0.5?0:(c>0.5?2:1));
        int mode= m==0?0: m==1?0: m==2?1: m==3?2:3;
        int start= m==1? (int)(bl*5): (m==3? (int)(bl*6.2)-12 : 0); /* rough arrival ~ (d/9.5 m/s)*60 */
        (void)arr;
        R r=run(mates[mi],mode,d,start<0?0:start,sp?WP_BTN_SPRINT:0,hold);
        n[cat]++; if(!r.got) f[cat]++;
      }
    }
    printf(" %-34s  toward-ball: %2d/%2d missed | sideways: %2d/%2d missed | forward/with-ball: %2d/%2d missed\n",MN[m],f[0],n[0],f[1],n[1],f[2],n[2]);
   }
  }
}
