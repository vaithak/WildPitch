#include "pt.h"
static SimState s;
int main(int argc,char**argv){ int H=atoi(argv[1]); int sy=argc>2?atoi(argv[2]):127;
    setup_empty(&s,3); s.seat_player[0]=3; s.players[3].pos=fv2_make(-FX(10),-FX(8)); s.ball.pos=fv2_make(-FX(9.6),-FX(8));
    for(int i=0;i<5;i++) step1(&s,(WpSeatInput){0,0,0});
    for(int i=0;i<H;i++){ step1(&s,(WpSeatInput){0,(int8_t)sy,WP_BTN_PASS}); printf("hold t%d p(%.2f,%.2f) v(%.1f,%.1f) ball(%.2f,%.2f) bv(%.1f,%.1f) own=%d act=%d ch=%d ev=%x\n",i,tod(s.players[3].pos.x),tod(s.players[3].pos.y),tod(s.players[3].vel.x),tod(s.players[3].vel.y),tod(s.ball.pos.x),tod(s.ball.pos.y),tod(s.ball.vel.x),tod(s.ball.vel.y),s.ball.owner,s.players[3].action,s.players[3].kick_charge,s.events);}
    for(int t=0;t<10;t++){ step1(&s,(WpSeatInput){0,0,0}); printf("rel t%d p(%.2f,%.2f) ball(%.2f,%.2f) bv(%.1f,%.1f) own=%d act=%d q? ev=%x\n",t,tod(s.players[3].pos.x),tod(s.players[3].pos.y),tod(s.ball.pos.x),tod(s.ball.pos.y),tod(s.ball.vel.x),tod(s.ball.vel.y),s.ball.owner,s.players[3].action,s.events);}
}
