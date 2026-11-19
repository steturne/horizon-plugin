/* ---- THE SKY: copied unchanged from Mixtape 2's HORIZON (mixtape2/Devices/horizon.js): the shader and its WebGL2 runner ---- */
const SKY_TAB=new Uint8Array(256);{let s=20261119;for(let i=0;i<256;i++){s=(Math.imul(s,1664525)+1013904223)>>>0;SKY_TAB[i]=s>>>24;}}   /* the random numbers the noise is made of */
const SKY_FS=`#version 300 es
precision highp float;
uniform vec2 uRes;uniform float uT,uAsp,uH,uAur,uLev,uPulse,uPh,uPhoto,uY0;uniform vec3 uAA,uAB,uAC,uAP;uniform vec4 uSw[4];
uniform sampler2D uTab,uImg,uSky;uniform vec2 uImgSize,uFit,uFire;
out vec4 o;
float h1(float i){return texelFetch(uTab,ivec2(int(mod(i,256.)),0),0).r;}
float n1(float x){float i=floor(x),f=x-i;f=f*f*(3.-2.*f);return mix(h1(i),h1(i+1.),f);}
float h2(vec2 p){p=fract(p*vec2(123.34,456.21));p+=dot(p,p+45.32);return fract(p.x*p.y);}
float n2(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(h2(i),h2(i+vec2(1,0)),f.x),mix(h2(i+vec2(0,1)),h2(i+1.),f.x),f.y);}
float fbm(vec2 p){float s=0.,a=.5;for(int i=0;i<4;i++){s+=a*n2(p);p=p*2.03+vec2(17.1,9.2);a*=.5;}return s;}
float luma(vec3 c){return dot(c,vec3(.3,.59,.11));}
/* the cloud aurora: noise bent by noise, drifting; patches pulsing; lit by the sweeps (swD) */
vec3 cloudAur(vec2 q,float swD){float e=q.y-uH;float band=smoothstep(.1,.32,e)*smoothstep(1.1,.5,e);if(band<.001)return vec3(0.);
  vec2 p=q*vec2(1.5,2.4);vec2 w=vec2(fbm(p+vec2(uT*.025,0.)),fbm(p+vec2(5.2,1.3)-vec2(0.,uT*.02)));
  float n=fbm(p*1.25+w*1.9+vec2(uT*.035,uT*.012)),shape=smoothstep(.42,.82,n);
  float pp=n2(vec2(q.x*2.2+uT*.04,uT*.3+q.y*1.5));
  float I=shape*band*(uAur*.22+uLev*.4+uPulse*.2+swD*2.4)*(.35+1.1*pp*pp);
  vec3 c=mix(uAA*vec3(.75,1.,.9),uAP,smoothstep(.35,.8,n2(q*1.4+vec2(uT*.02,3.))));c=mix(c,uAC,smoothstep(.45,.95,e)*.6);
  return c*I*.8;}
/* the curtains: three, each a winding hem with rays rising from it; q in screen heights (x 0…uAsp, y 0 at the bottom) */
vec3 aurora(vec2 q){vec3 acc=vec3(0.);
  float sw=0.,swD=0.;for(int i=0;i<4;i++){float d=(q.x-uSw[i].x)/uSw[i].z,d2=d/2.2;float a=uSw[i].y*exp(-d*d);if(uSw[i].w<.5)sw+=a;swD+=uSw[i].y*exp(-d2*d2);}
  for(int k=0;k<3;k++){float fk=float(k);
    float wx=q.x+.16*(n1(q.x*1.6+uT*.07+fk*3.)-.5)+.07*(n1(q.x*4.1-uT*.13+fk*7.)-.5);   /* folds */
    float yb=uH+.2+.085*fk+.2*(n1(wx*1.05+fk*5.3+uT*.03)-.5)+.06*(n1(wx*3.3-uT*.09+fk)-.5)+.08*pow(n1(wx*.6+fk*9.-uT*.02),3.)+.02*uPulse*sin(q.x*7.-uPh*2.4+fk*1.7);
    float d=q.y-yb;if(d<-.07)continue;
    /* rays lean toward a point high above the middle (as rays seen in perspective do), more or less as the ribbon twists, and bend */
    float lean=-(q.x-.5*uAsp)*.38+.5*(n1(wx*1.3+uT*.05+fk*4.)-.5),bend=1.2*(n1(wx*.9-uT*.04+fk*6.)-.5);
    float sl=wx+d*lean+d*d*bend;
    float run=uPh*(.9+.3*fk)+2.5*n1(sl*.6+uT*.05+fk*11.);
    float rays=n1(sl*(20.+fk*8.)+run);rays=.06+1.9*pow(rays,3.2);rays*=.55+.9*pow(n1(sl*64.-run*2.2+fk*9.),1.5);
    float env=smoothstep(.2,.78,n1(wx*.85+fk*2.7+uT*.022+40.));
    float pat=n2(vec2(wx*4.5+fk*13.,uT*.45+fk*3.));env*=.35+1.3*pat*pat;
    float fl=sw*(.8+.4*pat);
    float H=(.17+.1*fk+.14*env)*(.7+.55*uLev+.35*fl),up=exp(-max(d,0.)/H),hem=smoothstep(-.06,0.,d),edge=1.+(1.3+uPulse+fl)*exp(-max(d,0.)/.014);
    float I=rays*(env*.85+.12)*up*hem*edge*(uAur+uLev*.9+uPulse*.35+fl*1.5);
    float purp=smoothstep(.5,.85,n2(vec2(wx*1.2+fk*4.,uT*.035+fk)));
    vec3 c=mix(uAA,uAP,purp*.75);c=mix(c,mix(uAB,uAP,purp*.5),smoothstep(0.,.55,d/H));c=mix(c,uAC,smoothstep(.4,1.4,d/H));
    c=mix(c,uAP*1.25,exp(-max(d,0.)/.01)*clamp(uPulse*.5+fl*.9+uLev*.25,0.,.85));
    acc+=c*I*(.6-.13*fk);}
  acc+=cloudAur(q,swD);
  return (1.-exp(-acc*1.9))*1.15+mix(uAA,uAP,.35)*swD*.04;}
/* without the picture: a plain night, stars, the aurora, dark ground */
vec3 night(vec2 q){float e=q.y-uH,g=clamp(e/(1.-uH),0.,1.);vec3 col=mix(vec3(.05,.2,.19),vec3(.02,.07,.08),smoothstep(0.,.2,g));col=mix(col,vec3(.008,.035,.05),smoothstep(.1,.55,g));col=mix(col,vec3(.004,.01,.025),smoothstep(.45,1.,g));
  vec2 p=q*90.,c=floor(p);float h=h2(c);if(h>.93){float d=length(fract(p)-.5-(vec2(h2(c+3.),h2(c+7.))-.5)*.6)/90.*uRes.y;col+=vec3(.85,.9,1.)*exp(-d*d)*(.3+.7*h2(c+2.))*(.6+.4*sin(uT*(1.+3.*h2(c+5.))+h*40.));}
  if(e<0.)return vec3(.008,.01,.014)+aurora(vec2(q.x,2.*uH-q.y))*.12;
  return col+aurora(q);}
/* the picture: where the sky ends in each column (uSky), in the picture's pixels (y down); 1 = sky */
float skyY(float x){float i=clamp(floor(x),0.,uImgSize.x-2.),f=clamp(x-i,0.,1.);return mix(texelFetch(uSky,ivec2(int(i),0),0).r,texelFetch(uSky,ivec2(int(i)+1,0),0).r,f);}
float reflY(float x){float i=clamp(floor(x),0.,uImgSize.x-2.),f=clamp(x-i,0.,1.);return mix(texelFetch(uSky,ivec2(int(i),1),0).r,texelFetch(uSky,ivec2(int(i)+1,1),0).r,f);}
float skyMask(vec2 p){return smoothstep(-1.2,1.2,skyY(p.x)-p.y-.5);}
vec3 photo(vec2 uv){vec2 p=vec2(.5+(uv.x-.5)*uFit.x,.5-(uv.y-.5)*uFit.y)*uImgSize,t=p/uImgSize;
  vec3 c=texture(uImg,t).rgb;float sm=skyMask(p);
  /* its stars twinkle: a star is brighter than all four of its neighbours (an edge of the sky is not) */
  vec2 o2=2./uImgSize;vec3 mx=max(max(texture(uImg,t+vec2(o2.x,0.)).rgb,texture(uImg,t-vec2(o2.x,0.)).rgb),max(texture(uImg,t+vec2(0.,o2.y)).rgb,texture(uImg,t-vec2(0.,o2.y)).rgb));
  vec3 st=max(c-mx,0.)*1.6;float ch=h2(floor(p*.5));float tw=.6+.4*sin(uT*(1.2+3.5*ch)+ch*60.+uPh*ch*3.);
  c+=st*sm*((tw-1.)*.9+uLev*.35*tw);
  vec3 col=c;
  if(sm>.001)col+=aurora(vec2(uv.x*uAsp,uv.y))*sm;
  col+=mix(uAA,uAP,.25)*luma(c)*(1.-sm)*smoothstep(1.,5.,p.y-skyY(p.x))*(uAur*.3+uLev*.7)*step(p.y,uY0);   /* its light on the rock and snow */
  /* the lake: the picture's reflections are stretched and lower than a true mirror's, so the aurora is mapped onto them: where the
     reflected mountains end in each column (row 1 of uSky) is where the skyline is, and down the water the sky goes up (a little
     faster: the reflections are stretched by about 1.17); as bright as water reflects (Fresnel): near-mirror at the far shore, looking
     across, about 90%, half as the water comes near */
  if(p.y>uY0+3.){float w=(n2(vec2(p.x*.05,p.y*.35-uT*.6))-.5)*2.5*(1.+(p.y-uY0)*.012),x=p.x+w,below=p.y-reflY(x),rs=smoothstep(-1.5,1.5,below);
    if(rs>.001){float ys=skyY(x)-below*.85,my=.5-(ys/uImgSize.y-.5)/uFit.y;col+=aurora(vec2(uv.x*uAsp,my))*rs*mix(.9,.5,clamp((p.y-uY0)/(uImgSize.y-uY0),0.,1.));}}
  float fl=.62+.38*n1(uT*7.3)+.18*n1(uT*19.+5.);vec2 f2=vec2(uFire.x,2.*uY0-uFire.y),a=p-uFire,b=p-f2;   /* the campfire */
  col+=vec3(1.,.5,.18)*(exp(-dot(a,a)/30.)+.25*exp(-dot(a,a)/900.)+.6*exp(-dot(b,b)/50.))*(fl-.8)*.9;
  return col;}
void main(){vec2 uv=gl_FragCoord.xy/uRes;vec3 col=uPhoto>.5?photo(uv):night(vec2(uv.x*uAsp,uv.y));
  col*=1.-.22*pow(length((uv-.5)*vec2(1.,1.2)),2.2);
  col+=(h2(gl_FragCoord.xy+fract(uT*7.))-.5)/255.;o=vec4(col,1.);}`;
function skyGL(cv){const gl=cv.getContext('webgl2',{antialias:false,alpha:false,depth:false,preserveDrawingBuffer:false,powerPreference:'low-power'});if(!gl)throw new Error('no WebGL2');
  const sh=(type,src)=>{const s=gl.createShader(type);gl.shaderSource(s,src);gl.compileShader(s);if(!gl.getShaderParameter(s,gl.COMPILE_STATUS))throw new Error(gl.getShaderInfoLog(s));return s;};
  const pg=gl.createProgram();gl.attachShader(pg,sh(gl.VERTEX_SHADER,'#version 300 es\nin vec2 p;void main(){gl_Position=vec4(p,0.,1.);}'));gl.attachShader(pg,sh(gl.FRAGMENT_SHADER,SKY_FS));gl.linkProgram(pg);
  if(!gl.getProgramParameter(pg,gl.LINK_STATUS))throw new Error(gl.getProgramInfoLog(pg));gl.useProgram(pg);
  const vb=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,vb);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array([-1,-1,3,-1,-1,3]),gl.STATIC_DRAW);const pl=gl.getAttribLocation(pg,'p');gl.enableVertexAttribArray(pl);gl.vertexAttribPointer(pl,2,gl.FLOAT,false,0,0);
  const tex=(unit,fn)=>{const t=gl.createTexture();gl.activeTexture(gl.TEXTURE0+unit);gl.bindTexture(gl.TEXTURE_2D,t);fn();gl.activeTexture(gl.TEXTURE0);return t;};
  const nearest=()=>{gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.NEAREST);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.NEAREST);};
  tex(0,()=>{gl.pixelStorei(gl.UNPACK_ALIGNMENT,1);gl.texImage2D(gl.TEXTURE_2D,0,gl.R8,256,1,0,gl.RED,gl.UNSIGNED_BYTE,SKY_TAB);nearest();});
  const U={};for(const n of ['uRes','uT','uAsp','uH','uAur','uLev','uPulse','uPh','uPhoto','uY0','uAA','uAB','uAC','uAP','uSw','uTab','uImg','uSky','uImgSize','uFit','uFire'])U[n]=gl.getUniformLocation(pg,n);
  gl.uniform1i(U.uTab,0);gl.uniform1i(U.uImg,1);gl.uniform1i(U.uSky,2);
  gl.uniform3f(U.uAA,.2,1,.55);gl.uniform3f(U.uAB,.08,.8,.72);gl.uniform3f(U.uAC,.55,.22,.85);gl.uniform3f(U.uAP,.78,.28,1);gl.uniform1f(U.uAur,.385);
  let P=null;
  return {photo(ph){tex(1,()=>{gl.pixelStorei(gl.UNPACK_ALIGNMENT,4);gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,gl.RGBA,gl.UNSIGNED_BYTE,ph.im);
        for(const [k,v] of [[gl.TEXTURE_MIN_FILTER,gl.LINEAR],[gl.TEXTURE_MAG_FILTER,gl.LINEAR],[gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE],[gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE]])gl.texParameteri(gl.TEXTURE_2D,k,v);});
      const n=ph.sky.length,rows=new Float32Array(n*2);rows.set(ph.sky);for(let i=0;i<n;i++)rows[n+i]=ph.refl&&ph.refl[i]!=null?ph.refl[i]:2*ph.y0-ph.sky[i];   /* row 0 the skyline, row 1 the lake's reflected one (else a true mirror) */
      tex(2,()=>{gl.pixelStorei(gl.UNPACK_ALIGNMENT,1);gl.texImage2D(gl.TEXTURE_2D,0,gl.R32F,n,2,0,gl.RED,gl.FLOAT,rows);nearest();});
      P={w:ph.im.width,h:ph.im.height,y0:ph.y0,fire:ph.fire};},
    /* st: t (the drift's clock), ph (the rays' run), lev, pulse, sw (four surges: x, strength, width, cloud) */
    draw(st){const W=cv.width,H=cv.height,asp=W/H;gl.viewport(0,0,W,H);gl.uniform2f(U.uRes,W,H);gl.uniform1f(U.uT,st.t);gl.uniform1f(U.uAsp,asp);gl.uniform1f(U.uPh,st.ph);
      gl.uniform1f(U.uLev,st.lev);gl.uniform1f(U.uPulse,st.pulse);gl.uniform4fv(U.uSw,st.sw);gl.uniform1f(U.uPhoto,P?1:0);gl.uniform1f(U.uH,P?.29:.2);
      if(P){const s=Math.max(W/P.w,H/P.h);gl.uniform2f(U.uImgSize,P.w,P.h);gl.uniform2f(U.uFit,W/(P.w*s),H/(P.h*s));gl.uniform2f(U.uFire,P.fire[0],P.fire[1]);gl.uniform1f(U.uY0,P.y0);}   /* the picture covers the sky, cut evenly */
      gl.drawArrays(gl.TRIANGLES,0,3);},
    finish(){const b=new Uint8Array(4);gl.readPixels(0,0,1,1,gl.RGBA,gl.UNSIGNED_BYTE,b);},lost:   /* (reading a pixel back waits for the card) */()=>gl.isContextLost(),
    dispose(){const x=gl.getExtension('WEBGL_lose_context');if(x)x.loseContext();}};}
window.HZ_skyGL=skyGL;
