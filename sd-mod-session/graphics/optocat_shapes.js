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
    // The same fixed limb lengths are used in both resting and striking poses.
    const sx=160,sy=110;
    let tx=166+(190-166)*reach,ty=142+(by-142)*reach;
    let dx=tx-sx,dy=ty-sy,d=Math.hypot(dx,dy);const max=32;
    if(d>max){dx*=max/d;dy*=max/d;d=max;tx=sx+dx;ty=sy+dy;}
    const upper=16,lower=17,a=(upper*upper-lower*lower+d*d)/(2*d),h=Math.sqrt(Math.max(0,upper*upper-a*a));
    const ex=sx+a*dx/d-h*dy/d,ey=sy+a*dy/d+h*dx/d;
    c.lineCap='round';line([[sx+1,sy+2],[ex+1,ey+2],[tx+1,ty+2]],'#292929',12);
    const arm=c.createLinearGradient(sx-5,sy-8,tx+5,ty+8);arm.addColorStop(0,'#bebebe');arm.addColorStop(.5,'#909090');arm.addColorStop(1,'#484848');
    line([[sx,sy],[ex,ey],[tx,ty]],arm,10);line([[sx-2,sy-2],[ex-2,ey-2],[tx-2,ty-2]],'#b6b6b6',1);
    // The little dark paw bends down at the wrist, following the pose reference.
    c.save();c.translate(tx,ty);c.rotate(.15+.45*reach);
    oval(1,2,5.8,7,radial(-1,0,9,'#5c5c5c','#2b2b2b','#090909'));
    line([[0,3],[0,6]],'#1c1c1c',.8);line([[3,2],[3,5]],'#1c1c1c',.8);c.restore();
    if(impact>.3){line([[200,by-13],[204,by-18]],'#aaa');line([[207,by-3],[213,by-4]],'#999');}
  }
  function awake(by,reach,impact){
    const s=clamp(reach*1.15,0,1),mix=(a,b)=>a+(b-a)*s;
    oval(144,155,36,4,'#090909');
    // Shift from sitting to a supported, forward-reaching stance.
    path(()=>{c.moveTo(mix(144,126),mix(144,125));c.bezierCurveTo(mix(131,114),mix(149,122),mix(119,108),mix(153,110),mix(109,112),mix(148,95));c.quadraticCurveTo(mix(122,104),mix(150,104),mix(137,112),mix(140,127));c.closePath();},radial(mix(129,115),mix(145,119),25,'#737373','#353535','#111'));
    // One continuous hind leg: thigh, bent hock and a flat planted paw.
    // Its upper part is covered by the torso instead of forming a separate disc.
    path(()=>{
      c.moveTo(mix(150,124),mix(130,122));
      c.bezierCurveTo(mix(164,139),mix(129,122),mix(167,138),mix(140,132),mix(161,131),mix(145,139));
      c.lineTo(mix(158,124),mix(147,146));
      c.quadraticCurveTo(mix(166,133),mix(148,149),mix(166,132),mix(152,154));
      c.quadraticCurveTo(mix(159,122),mix(157,156),mix(151,116),mix(153,154));
      c.quadraticCurveTo(mix(146,113),mix(149,150),mix(150,117),mix(143,141));
      c.lineTo(mix(153,120),mix(137,133));
      c.quadraticCurveTo(mix(148,116),mix(134,126),mix(150,124),mix(130,122));c.closePath();
    },radial(mix(155,126),mix(139,133),25,'#bebebe','#8e8e8e','#444'));
    c.save();c.clip();c.fillStyle=radial(mix(157,124),149,14,'#505050','#272727','#090909');c.fillRect(110,146,61,12);c.restore();
    farForeleg(by,reach,impact);
    path(()=>{
      c.moveTo(mix(137,150),94);
      c.bezierCurveTo(mix(130,140),mix(109,108),mix(140,147),mix(121,117),mix(142,137),mix(129,116));
      c.quadraticCurveTo(mix(145,121),mix(140,110),mix(139,117),mix(149,124));
      c.quadraticCurveTo(mix(151,115),mix(157,137),mix(163,132),mix(150,139));
      c.bezierCurveTo(mix(169,147),mix(136,145),mix(165,161),mix(122,137),mix(158,160),mix(111,123));
      c.lineTo(mix(158,163),95);c.closePath();
    },radial(145,116,38,'#c8c8c8','#9b9b9b','#4a4a4a'));
    line([[mix(147,153),mix(126,125)],[mix(147,154),147]],radial(149,134,23,'#c2c2c2','#999','#444'),8);
    oval(mix(149,156),152,7,4,radial(mix(147,154),149,11,'#5c5c5c','#292929','#090909'));
    const headAngle=.025+.16*clamp((by-83)/71,0,1);
    kittenHead(mix(150,153),mix(77,74),headAngle-s*.035,by);
  }
