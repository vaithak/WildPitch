#include "pt.h"
#define ME 3
#define MATE 4
typedef struct { int got, who, t; double closest, relc, run, ftgap, ftdot, vin; int tc_block; } R2;
/* stick: dir index 0..7 (angle k*45deg) or -1 = keep, -2 neutral. delay ticks after kick. */
static R2 run(fv2 mate, int dir, int delay, uint16_t spr, int through_hold, int live){
  static SimState s; R2 r; memset(&r,0,sizeof r); r.closest=99; r.who=-9; r.t=-1;
  setup_empty(&s, ME); if(live) s.flags&=(uint8_t)~WP_SIMF_STATIC_OTHERS;
  s.seat_player[0]=ME; fv2 me=fv2_make(-FX(10),0); s.players[ME].pos=me; s.players[MATE].pos=fv2_add(me,mate);
  s.ball.pos=fv2_add(me,fv2_make(FX(0.4),0)); for(int i=0;i<5;i++) step1(&s,(WpSeatInput){0,0,0});
  WpSeatInput ps=stick_to(s.players[ME].pos,s.players[MATE].pos,0,1);
  int hold=through_hold?through_hold:3;
  for(int t=0;t<hold;t++){ WpSeatInput a=ps; a.buttons=WP_BTN_PASS|spr; step1(&s,a);} 
  int kt=-1; double prevd=99;
  for(int t=0;t<400;t++){
    WpSeatInput in=ps; in.buttons=spr;
    if(kt>=0 && t-kt>=delay){
      if(dir==-2){in.move_x=in.move_y=0;}
      else if(dir>=0){ double a=dir*M_PI/4; in.move_x=(int8_t)(fabs(cos(a))<0.1?0:(cos(a)>0?127:-127)); in.move_y=(int8_t)(fabs(sin(a))<0.1?0:(sin(a)>0?127:-127)); }
    }
    uint16_t ev=step1(&s,in);
    if((ev&WP_EV_KICK) && kt<0) kt=t;
    if(kt<0) continue;
    WpPlayer*q=&s.players[MATE];
    double d=bdist(&s,MATE);
    if(s.ball.owner<0 && d<r.closest){ r.closest=d; r.relc=spd(fv2_sub(s.ball.vel,q->vel)); if(q->touch_cooldown) r.tc_block=1; else r.tc_block=0;}
    r.run+=spd(q->vel)/60.0;
    if(s.ball.owner<0) r.vin=spd(s.ball.vel);
    if(s.ball.owner>=0){ r.got=s.ball.owner==MATE; r.who=s.ball.owner; r.t=t-kt;
       /* first touch: follow 20 ticks */
       double mx=0; fv2 b0=s.ball.pos; fv2 p0=q->pos;
       for(int k=0;k<20;k++){ step1(&s,in); double dd=bdist(&s,MATE); if(dd>mx)mx=dd; }
       r.ftgap=mx; fv2 bd=fv2_sub(s.ball.pos,b0); (void)p0;
       int has=in.move_x||in.move_y; if(has && fv2_len(bd)>FX(0.05)){ fv2 n=fv2_norm(bd); double sx=in.move_x/127.0, sy=in.move_y/127.0, l=sqrt(sx*sx+sy*sy); r.ftdot=(tod(n.x)*sx+tod(n.y)*sy)/l;} else r.ftdot=9;
       break; }
    if(s.phase==WP_PHASE_OUT){ r.who=-2; break;}
    if(t-kt>240) break;
    (void)prevd;
  }
  return r;
}
static const char* DN[]={"E(+x)","NE","N","NW","W","SW","S","SE"};
int main(int argc, char**argv){
  int live=argc>1 && argv[1][0]=='l';
  int through=argc>2?atoi(argv[2]):0;
  fv2 mates[]={{FX(12),0},{FX(10),FX(6)},{FX(5),FX(10)},{FX(12),-FX(5)},{FX(20),FX(3)}};
  int tot=0,fail=0;
  for(int m=0;m<5;m++){
    R2 base=run(mates[m],-1,0,0,through,live);
    printf("\nMATE at +(%.0f,%.0f): baseline keep-stick: got=%d t=%d vin=%.1f closest=%.2f run=%.1f\n",tod(mates[m].x),tod(mates[m].y),base.got,base.t,base.vin,base.closest,base.run);
    R2 nr=run(mates[m],-2,0,0,through,live);
    printf("   neutral from kick: got=%d t=%d closest=%.2f run=%.1f\n",nr.got,nr.t,nr.closest,nr.run);
    int arr=base.t>0?base.t:60;
    int delays[3]={0,arr/2,arr-12<0?0:arr-12};
    for(int sp=0;sp<2;sp++){
      printf("   %s  delay:   early(0)                      mid(%d)                        late(%d)\n",sp?"SPRINT":"jog   ",delays[1],delays[2]);
      for(int d=0;d<8;d++){
        printf("   %-6s ",DN[d]);
        for(int di=0;di<3;di++){ R2 r=run(mates[m],d,delays[di],sp?WP_BTN_SPRINT:0,through,live); tot++; if(!r.got) fail++;
          printf("| %s c=%4.2f rel=%4.1f run=%4.1f %s ", r.got?"GOT ":(r.who==-2?"OUT ":(r.who>=0?"OTH ":"MISS")), r.closest, r.relc, r.run, r.got?(r.ftdot>1?"":(r.ftdot>0.5?"ft+":"ft-")):"   "); }
        printf("\n");
      }
    }
  }
  printf("\nTOTAL stick-pressed receptions: %d/%d failed\n",fail,tot);
}
