#include "pt.h"
static SimState s;
int main(int argc,char**argv){ int sx=atoi(argv[1]), sy=atoi(argv[2]); int fromrecv=argc>3;
    setup_empty(&s,3); s.seat_player[0]=3;
    s.players[3].pos=fv2_make(-FX(10),0); s.players[4].pos=fv2_make(FX(2),0); s.ball.pos=fv2_make(-FX(9.6),0);
    for(int i=0;i<5;i++) step1(&s,(WpSeatInput){0,0,0});
    for(int i=0;i<3;i++) step1(&s,(WpSeatInput){127,0,WP_BTN_PASS});
    WpSeatInput in={127,0,0}; int got=-1;
    for(int t=0;t<200;t++){ if(s.seat_player[0]==4 && !fromrecv && bdist(&s,4)<0.9){in.move_x=(int8_t)sx;in.move_y=(int8_t)sy;}
      step1(&s,in); if(s.ball.owner==4){got=t;break;} }
    printf("received at gap %.2f, ball v=(%.1f,%.1f)\n",bdist(&s,4),tod(s.ball.vel.x),tod(s.ball.vel.y));
    in.move_x=(int8_t)sx; in.move_y=(int8_t)sy;
    for(int k=1;k<=40;k++){ step1(&s,in); if(k%3==0) printf(" +%2d p(%.2f,%.2f) v(%.1f,%.1f)  ball(%.2f,%.2f) v(%.1f,%.1f) gap %.2f own=%d tc=%d ev=%x\n",k,tod(s.players[4].pos.x),tod(s.players[4].pos.y),tod(s.players[4].vel.x),tod(s.players[4].vel.y),tod(s.ball.pos.x),tod(s.ball.pos.y),tod(s.ball.vel.x),tod(s.ball.vel.y),bdist(&s,4),s.ball.owner,s.players[4].touch_cooldown,s.events);}
}
