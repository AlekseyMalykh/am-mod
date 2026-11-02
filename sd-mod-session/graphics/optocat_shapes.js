  function radial(x,y,r,light='#ddd',mid='#929292',dark='#292929'){
    const g=c.createRadialGradient(x-r*.3,y-r*.35,r*.03,x,y,r);g.addColorStop(0,light);g.addColorStop(.48,mid);g.addColorStop(1,dark);return g;
  }
  function path(draw,fill,stroke,width=1){c.beginPath();draw();if(fill){c.fillStyle=fill;c.fill();}if(stroke){c.strokeStyle=stroke;c.lineWidth=width;c.stroke();}}
  function oval(x,y,rx,ry,fill){path(()=>c.ellipse(x,y,rx,ry,0,0,Math.PI*2),fill);}
  function line(points,color,width=1){path(()=>points.forEach((p,i)=>i?c.lineTo(...p):c.moveTo(...p)),null,color,width);}
  function poly(points,fill,stroke){path(()=>{points.forEach((p,i)=>i?c.lineTo(...p):c.moveTo(...p));c.closePath();},fill,stroke);}
  function bow(y,impact){
    const bx=194+impact*4;
    path(()=>{c.moveTo(207,19);c.quadraticCurveTo(201+impact*8,(19+y)/2,bx,y);},null,'#929292',.8);
    c.save();c.translate(bx,y);c.rotate(-.12-impact*.23);
    poly([[0,0],[-11,-7],[-12,7],[-3,4]],radial(-6,-2,13,'#f4f4f4','#b2b2b2','#4c4c4c'),'#bcbcbc');
    poly([[0,0],[11,-7],[12,7],[3,4]],radial(4,-2,13,'#e7e7e7','#999','#414141'),'#aaa');
    poly([[-2,2],[-7,13],[-2,10],[0,13],[3,2]],'#9d9d9d');
    oval(0,1,3,4,radial(-.8,-1,5,'#eee','#aaa','#444'));
    line([[-9,-3],[-3,0]],'#888',.7);line([[4,0],[9,-3]],'#666',.7);
    c.restore();
  }
  function kittenHead(x,y,angle,by,asleep=false,scale=1){
    c.save();c.translate(x,y);c.rotate(angle);c.scale(scale,scale);
    // Tall, softly pointed ears, a round head and a dark facial mask.
    path(()=>{c.moveTo(-28,-7);c.bezierCurveTo(-32,-27,-26,-50,-17,-56);c.quadraticCurveTo(-10,-56,-3,-23);c.closePath();},radial(-17,-34,30,'#cacaca','#888','#292929'),'#373737',1);
    path(()=>{c.moveTo(4,-22);c.quadraticCurveTo(11,-48,22,-55);c.quadraticCurveTo(34,-45,32,-10);c.closePath();},radial(17,-30,28,'#ccc','#8d8d8d','#222'),'#252525',1);
    path(()=>{c.moveTo(-26,-21);c.quadraticCurveTo(-25,-43,-17,-51);c.quadraticCurveTo(-11,-42,-8,-22);c.closePath();},'#c4c4c4');
    path(()=>{c.moveTo(10,-22);c.quadraticCurveTo(16,-44,22,-49);c.quadraticCurveTo(28,-37,27,-19);c.closePath();},'#b9b9b9');
    oval(0,0,32,29,radial(-8,-9,41,'#d3d3d3','#a0a0a0','#4b4b4b'));
    // Three-quarter view to the right: face shifted right within the head.
    path(()=>{c.moveTo(-13,-14);c.bezierCurveTo(1,-24,24,-22,30,-7);c.bezierCurveTo(36,8,29,25,17,26);c.bezierCurveTo(1,28,-18,16,-17,2);c.quadraticCurveTo(-18,-8,-13,-14);c.closePath();},radial(4,-1,34,'#414141','#272727','#0e0e0e'));
    if(asleep){
      path(()=>{c.moveTo(-4,7);c.quadraticCurveTo(2,12,8,6);},null,'#bdbdbd',1.5);
      path(()=>{c.moveTo(18,6);c.quadraticCurveTo(22,10,26,4);},null,'#bdbdbd',1.4);
    }else{
      const gx=(194-x)*Math.cos(angle)+(by-y)*Math.sin(angle),gy=-(194-x)*Math.sin(angle)+(by-y)*Math.cos(angle);
      function eye(ex,ey,rx,ry){
        const eyeScale=1+Number(gain.value)/18*.35;rx*=eyeScale;ry*=eyeScale;
        oval(ex,ey,rx+1,ry+1,'#0b0b0b');
        oval(ex,ey,rx,ry,radial(ex-2,ey-3,ry+3,'#f3f3f3','#c5c5c5','#646464'));
        c.save();c.beginPath();c.ellipse(ex,ey,rx,ry,0,0,Math.PI*2);c.clip();
        const a=Math.atan2(gy-ey,gx-ex),px=ex+Math.cos(a)*2.1,py=ey+Math.sin(a)*1.8;
        oval(px,py,3.5*eyeScale,6.4*eyeScale,'#0a0a0a');oval(px-1.1,py-2.8,1.3*eyeScale,1.9*eyeScale,'#fff');oval(px+1,py+2.8,.65,.8,'#a7a7a7');c.restore();
      }
      eye(2,3,7,10);eye(23,3,4.8,8.4);
    }
    // Minimal dark nose and mouth, without pale muzzle patches.
    poly([[19,15],[25,15],[22,19]],'#050505');
    path(()=>{c.moveTo(22,18.5);c.lineTo(22,21);c.quadraticCurveTo(20,23,18,21);},null,'#101010',.8);
    c.restore();
  }
  function sleep(){
    oval(145,153,43,5,'#090909');
    oval(141,133,34,22,radial(134,124,40,'#c2c2c2','#939393','#444'));
    oval(154,147,9,5,radial(151,145,12,'#5a5a5a','#303030','#111'));
    kittenHead(169,132,.18,133,true,.62);
    const tail=c.createLinearGradient(113,131,139,153);tail.addColorStop(0,'#606060');tail.addColorStop(.5,'#303030');tail.addColorStop(1,'#111');
    path(()=>{c.moveTo(115,127);c.bezierCurveTo(102,143,120,158,147,151);c.quadraticCurveTo(155,149,153,144);},null,tail,10);
    path(()=>{c.moveTo(112,131);c.bezierCurveTo(108,141,121,149,139,148);},null,'#777',.8);
    // Sleep letters climb diagonally, like the original duck's indicator.
    c.fillStyle='#c4c4c4';c.font='13px monospace';c.fillText('Z',199,121);
    c.font='15px monospace';c.fillText('z',213,106);
    c.font='17px monospace';c.fillText('z',228,89);
  }
  function farForeleg(by,reach,impact){
    // Draw the far foreleg before the torso, so its shoulder is occluded.
    // Lift into a folded wrist before extending; never lengthen either bone.
    const ease=t=>t*t*(3-2*t),lean=ease(clamp(reach/.7,0,1));
    const sx=160+4*lean,sy=111-2*lean;
    const lift=ease(clamp(reach/.375,0,1)),strike=ease(clamp((reach-.375)/.625,0,1));
    let tx=166+5*lift+19*strike,ty=145-21*lift+(by-124)*strike;
    let dx=tx-sx,dy=ty-sy,d=Math.hypot(dx,dy);const max=32;
    if(d>max){dx*=max/d;dy*=max/d;d=max;tx=sx+dx;ty=sy+dy;}
    const upper=16,lower=17,a=(upper*upper-lower*lower+d*d)/(2*d),h=Math.sqrt(Math.max(0,upper*upper-a*a));
    const ex=sx+a*dx/d-h*dy/d,ey=sy+a*dy/d+h*dx/d;
    c.lineCap='round';line([[sx+1,sy+2],[ex+1,ey+2],[tx+1,ty+2]],'#292929',12);
    const arm=c.createLinearGradient(sx-5,sy-8,tx+5,ty+8);arm.addColorStop(0,'#bebebe');arm.addColorStop(.5,'#909090');arm.addColorStop(1,'#484848');
    line([[sx,sy],[ex,ey],[tx,ty]],arm,10);line([[sx-2,sy-2],[ex-2,ey-2],[tx-2,ty-2]],'#b6b6b6',1);
    // The little dark paw bends down at the wrist, following the pose reference.
    c.save();c.translate(tx,ty);c.rotate(.15+.45*strike);
    oval(1,2,5.8,7,radial(-1,0,9,'#5c5c5c','#2b2b2b','#090909'));
    line([[0,3],[0,6]],'#1c1c1c',.8);line([[3,2],[3,5]],'#1c1c1c',.8);c.restore();
    if(impact>.3){line([[200,by-13],[204,by-18]],'#aaa');line([[207,by-3],[213,by-4]],'#999');}
  }
  function awake(by,reach,impact){
    const t=clamp(reach/.7,0,1),s=t*t*(3-2*t),lean=4*s;
    oval(144,155,36,4,'#090909');
    // The seated pelvis, hind feet and tail root stay on the same ground points.
    path(()=>{c.moveTo(129,140);c.bezierCurveTo(119,149,108,152,102,149);c.quadraticCurveTo(113,157,133,148,137,144);c.closePath();},radial(120,145,22,'#777','#414141','#171717'));
    oval(145,151,8,3.5,radial(143,149,12,'#555','#303030','#111'));
    farForeleg(by,reach,impact);
    // Only the chest leans over the supporting front foot; the rump stays seated.
    path(()=>{
      c.moveTo(137+lean,94-s);
      c.bezierCurveTo(135+lean,110,126,118,122,130);
      c.bezierCurveTo(116,143,126,151,137,150);
      c.quadraticCurveTo(150,149,153,137);
      c.bezierCurveTo(159,128,162+lean,115-s,158+lean,105-s);
      c.lineTo(158+lean,94-s);c.closePath();
    },radial(142+lean,117,38,'#c8c8c8','#9b9b9b','#4a4a4a'));
    // A quiet belly edge separates the abdomen from the supporting foreleg.
    path(()=>{c.moveTo(145+lean*.5,120);c.quadraticCurveTo(151,133,143,143);},null,'#777',.8);
    // Continuous folded hind limb: rounded thigh into a short, planted dark foot.
    path(()=>{
      c.moveTo(129,124);c.bezierCurveTo(119,125,115,136,120,144);
      c.quadraticCurveTo(123,150,131,151);c.lineTo(137,153);
      c.quadraticCurveTo(145,155,146,151);c.quadraticCurveTo(145,147,136,147);
      c.quadraticCurveTo(134,143,137,139);c.quadraticCurveTo(144,131,136,126);
      c.quadraticCurveTo(132,124,129,124);c.closePath();
    },radial(126,132,24,'#bcbcbc','#939393','#4b4b4b'));
    c.save();c.clip();c.fillStyle=radial(137,149,12,'#555','#2c2c2c','#101010');c.fillRect(126,147,22,10);c.restore();
    path(()=>{c.moveTo(130,134);c.quadraticCurveTo(135,137,132,143);},null,'#7c7c7c',.7);
    // Fixed front paw; the small chest shift changes the leg angle, not its length.
    path(()=>{c.moveTo(151+lean,119-s);c.quadraticCurveTo(153+lean,132,153,147);},null,radial(151,132,25,'#c6c6c6','#999','#494949'),8);
    oval(154,152,7,4,radial(152,149,11,'#5c5c5c','#292929','#090909'));
    const headAngle=.025+.16*clamp((by-83)/71,0,1);
    kittenHead(150+lean,77-s,headAngle-s*.025,by);
  }
