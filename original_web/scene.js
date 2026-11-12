import * as THREE from './vendor/three.module.js';
import { SVGRenderer } from './vendor/SVGRenderer.js';
import { OrbitControls } from './vendor/OrbitControls.js';

export class FarmScene {
  constructor(container,onSelect){
    this.container=container;this.onSelect=onSelect;this.labels=[];this.routes=[];this.pulses=[];this.labelsVisible=true;this.networkView=false;this.state={pump:false,tank:69,lamp:false,soil:46};this.lastPacket=0;this.live=false;this.stale=false;
    this.scene=new THREE.Scene();this.scene.background=new THREE.Color('#e9eddf');this.scene.fog=new THREE.Fog('#e9eddf',65,125);
    this.camera=new THREE.PerspectiveCamera(36,1,.1,200);this.camera.position.set(41,38,45);
    try{this.renderer=new THREE.WebGLRenderer({antialias:true,alpha:false});this.renderer.setPixelRatio(Math.min(window.devicePixelRatio,1.8));this.renderer.shadowMap.enabled=true;this.renderer.shadowMap.type=THREE.PCFSoftShadowMap;}catch{this.software=true;this.renderer=new SVGRenderer();this.renderer.setQuality('low');this.renderer.setPrecision(2);this.renderer.domElement.style.display='block';}this.renderer.outputColorSpace=THREE.SRGBColorSpace;this.renderer.toneMapping=THREE.ACESFilmicToneMapping;this.renderer.toneMappingExposure=1.28;container.appendChild(this.renderer.domElement);
    this.controls=new OrbitControls(this.camera,this.renderer.domElement);this.controls.target.set(0,1,-1);this.controls.enableDamping=true;this.controls.dampingFactor=.08;this.controls.maxPolarAngle=Math.PI*.47;this.controls.minDistance=19;this.controls.maxDistance=85;this.controls.maxTargetRadius=15;
    this.scene.add(new THREE.HemisphereLight(0xf5ffe7,0x8d8061,2.8));const sun=new THREE.DirectionalLight(0xfff0ce,3.1);sun.position.set(-15,30,20);sun.castShadow=true;sun.shadow.mapSize.set(2048,2048);sun.shadow.camera.left=-33;sun.shadow.camera.right=33;sun.shadow.camera.top=30;sun.shadow.camera.bottom=-30;sun.shadow.normalBias=.04;sun.shadow.bias=-.0003;this.scene.add(sun);this.sun=sun;if(this.software){sun.intensity=.65;this.scene.add(new THREE.AmbientLight('#ffffff',.85));}
    this.build();this.resize();new ResizeObserver(()=>this.resize()).observe(container);this.clock=new THREE.Clock();this.animate();
  }
  mat(color,extra={}){return this.software?new THREE.MeshLambertMaterial({color,...extra}):new THREE.MeshStandardMaterial({color,roughness:.85,...extra});}
  box(w,h,d,color,x,y,z,parent=this.scene){const o=new THREE.Mesh(new THREE.BoxGeometry(w,h,d),this.mat(color));o.position.set(x,y,z);o.castShadow=true;o.receiveShadow=true;if(this.software&&h<1&&y<.3)o.renderOrder=-5;parent.add(o);return o;}
  cylinder(r1,r2,h,color,x,y,z,parent=this.scene,sides=12){const o=new THREE.Mesh(new THREE.CylinderGeometry(r1,r2,h,sides),this.mat(color));o.position.set(x,y,z);o.castShadow=true;o.receiveShadow=true;parent.add(o);return o;}
  ball(r,color,x,y,z,parent=this.scene,scale=null){const o=new THREE.Mesh(new THREE.IcosahedronGeometry(r,this.software?0:1),this.mat(color));o.position.set(x,y,z);if(scale)o.scale.set(...scale);o.castShadow=true;parent.add(o);return o;}
  pipe(points,color,r=.045,parent=this.scene){const curve=new THREE.CatmullRomCurve3(points.map(p=>new THREE.Vector3(...p)));const mesh=new THREE.Mesh(new THREE.TubeGeometry(curve,Math.max(8,points.length*(this.software?3:8)),r,6,false),this.mat(color));mesh.castShadow=true;parent.add(mesh);return curve;}
  label(text,pos,type,id){const el=document.createElement('button');el.className='object-label '+type;el.innerHTML='<i></i>'+text;el.addEventListener('click',()=>this.onSelect(id));document.querySelector('#labels').appendChild(el);this.labels.push({el,point:new THREE.Vector3(...pos),id});}
  route(points,color,type){const curve=new THREE.CatmullRomCurve3(points.map(p=>new THREE.Vector3(...p)));const line=new THREE.Line(new THREE.BufferGeometry().setFromPoints(curve.getPoints(70)),new THREE.LineDashedMaterial({color,transparent:true,opacity:.55,dashSize:.3,gapSize:.2}));line.computeLineDistances();this.scene.add(line);this.routes.push({curve,line,type});for(let i=0;i<3;i++){const dot=new THREE.Mesh(new THREE.SphereGeometry(.13,8,6),new THREE.MeshBasicMaterial({color}));this.scene.add(dot);this.pulses.push({dot,curve,offset:i/3,type});}}
  build(){
    // One representative sector. Distances are visual, not survey coordinates.
    const base=this.box(42,.85,32,'#b3bb88',0,-.48,0);base.material.roughness=1;
    this.box(41.6,.12,31.6,'#b9c998',0,0,0);
    const shadowPlane=new THREE.Mesh(new THREE.PlaneGeometry(300,300),this.mat('#e9eddf'));shadowPlane.rotation.x=-Math.PI/2;shadowPlane.position.y=-1;shadowPlane.renderOrder=-100;shadowPlane.receiveShadow=true;this.scene.add(shadowPlane);
    this.box(5.1,.06,31,'#d4c49d',8,.1,0);this.box(40,.06,3.4,'#d4c49d',0,.12,-11.4);
    // Timber fence frames the cultivated sector.
    for(let x=-20;x<=20;x+=3.3){for(const z of [-15,15]){this.box(.14,1.25,.14,'#9a8964',x,.6,z);}}
    for(let z=-15;z<=15;z+=3){for(const x of [-20,20])this.box(.14,1.25,.14,'#9a8964',x,.6,z);}
    for(const z of [-15,15])for(const y of [.5,.95])this.box(40,.07,.07,'#b0a17d',0,y,z);
    for(const x of [-20,20])for(const y of [.5,.95])this.box(.07,.07,30,'#b0a17d',x,y,0);
    this.plants=[];this.leafMats=[this.mat('#4c7445'),this.mat('#60874a'),this.mat('#70944d')];
    const leafGeo=new THREE.IcosahedronGeometry(.67,this.software?0:1),berryGeo=new THREE.SphereGeometry(.07,5,4),berryMat=this.mat('#9e4b38');
    for(let row=0;row<6;row++){
      const x=-12+row*2.8;this.box(2.15,.11,18.5,'#a58e65',x,.15,2.4);
      this.pipe([[x,.27,-7],[x,.27,12]],'#313f32',.055);
      for(let col=0;col<7;col++){
        const z=-6+col*2.65;const g=new THREE.Group();g.position.set(x,0,z);this.scene.add(g);this.cylinder(.09,.13,1.25,'#79664a',0,.75,0,g,6);
        for(let n=0;n<5;n++){
          const leaf=new THREE.Mesh(leafGeo,this.leafMats[(row+col+n)%3]);const angle=n*2.4;
          leaf.position.set(Math.cos(angle)*.37,1.1+n*.16,Math.sin(angle)*.35);leaf.scale.set(1,.52,.8);leaf.rotation.z=Math.cos(n)*.3;leaf.rotation.y=angle;leaf.castShadow=true;g.add(leaf);
          for(let b=0;b<(this.software?0:2);b++){const berry=new THREE.Mesh(berryGeo,berryMat);berry.position.set(Math.cos(angle)*.44,1.15+n*.16+b*.08,Math.sin(angle)*.42);g.add(berry);}
        }
        this.plants.push(g);
      }
    }
    // Background shade trees.
    for(const [x,z,s] of [[-17,-4,1.1],[-17,8,1],[16,12,1.2],[1,-13,1.2],[17,-13,.9]]){const g=new THREE.Group();g.position.set(x,0,z);g.scale.setScalar(s);this.scene.add(g);this.cylinder(.16,.28,3.8,'#8b7959',0,1.9,0,g);for(const [a,b,c] of [[0,4.2,0],[-.8,3.8,.2],[.7,3.9,-.4]])this.ball(1.6,'#73945c',a,b,c,g,[1,.8,1]);}
    // Utility shelter; lighting is auxiliary, not crop growth lighting.
    this.box(5.8,.2,4.8,'#bcac84',-14,.15,-10.8);this.box(5.2,3.2,4,'#e0d4b3',-14,1.8,-10.8);
    const roof=new THREE.Mesh(new THREE.ConeGeometry(4.1,1.7,4),this.mat('#74876a'));roof.position.set(-14,4,-10.8);roof.rotation.y=Math.PI/4;roof.scale.z=.85;roof.castShadow=true;this.scene.add(roof);
    this.box(1.15,2.25,.1,'#83916e',-14,1.4,-8.75);this.box(1,.85,.12,'#a6c4bf',-12.45,2.2,-8.72);this.box(.07,.85,.15,'#f4e9c8',-12.45,2.2,-8.66);
    // Water tank with transparent wall and a visible level.
    this.cylinder(2.05,2.05,.25,'#acb6a2',13.3,.2,-6.3);
    const wall=this.cylinder(1.8,1.8,4.3,'#bbd7d6',13.3,2.48,-6.3);wall.material.transparent=true;wall.material.opacity=.27;wall.material.depthWrite=false;
    this.tankWater=this.cylinder(1.72,1.72,3.9,'#6aa6b0',13.3,2.25,-6.3,undefined,32);this.tankWater.material.transparent=true;this.tankWater.material.opacity=.75;
    this.cylinder(1.87,1.87,.15,'#6f8b88',13.3,4.7,-6.3);this.cylinder(.4,.4,.17,'#58736e',13.3,4.87,-6.3);
    for(let i=0;i<7;i++)this.cylinder(1.82,1.82,.025,'#94b4ac',13.3,.5+i*.61,-6.3,undefined,32);
    this.box(.65,.18,.35,'#47747d',13.3,4.88,-5.75);for(const x of [13.12,13.48]){const echo=this.cylinder(.13,.13,.2,'#c2d3cd',x,4.88,-5.56,undefined,16);echo.rotation.x=Math.PI/2;}
    // Pump and supply lines.
    this.pipe([[13.3,.48,-6.3],[13.3,.48,-2],[11,.48,-2],[10,.48,-2],[8,.48,-2],[8,.3,12.3],[-12,.3,12.3]],'#546c69',.14);
    this.box(1.7,.13,1.1,'#b6b8a1',12,.2,-2);this.box(1.25,.75,.8,'#608f97',12,.63,-2);this.pumpRotor=this.cylinder(.32,.32,.15,'#3c6970',12,.68,-1.53,undefined,12);this.pumpRotor.rotation.x=Math.PI/2;
    for(let row=0;row<6;row++)this.pipe([[-12+row*2.8,.29,12.3],[-12+row*2.8,.29,11.8]],'#354538',.06);
    this.drops=[];for(let row=0;row<6;row++)for(let k=0;k<7;k++){const d=new THREE.Mesh(new THREE.SphereGeometry(.05,5,4),new THREE.MeshBasicMaterial({color:'#84c9db',transparent:true,opacity:.8}));this.scene.add(d);this.drops.push({mesh:d,x:-12+row*2.8,z:-6+k*2.65,phase:(row+k*.3)%1});}
    // Edge controller with visibly wired sensor inputs.
    this.box(.22,2.2,.22,'#a99a77',-15,1.1,1);this.box(2.2,1.65,.7,'#f0ecdc',-15,2.5,1);this.box(1.85,1.3,.1,'#426957',-15,2.5,1.4);this.box(.75,.65,.14,'#34433b',-15,2.5,1.5);this.box(.55,.4,.16,'#b4b8ab',-15,2.96,1.48);
    for(let i=0;i<7;i++)for(const x of [-15.67,-14.33])this.box(.13,.07,.1,'#c9ccac',x,2.15+i*.12,1.54);
    this.controllerLed=this.ball(.08,'#b9ec75',-14.35,3,1.57);this.controllerLed.material.emissive=new THREE.Color('#9ed86c');this.controllerLed.material.emissiveIntensity=.5;
    this.box(.7,.48,.4,'#f4eee0',-15,1.9,1.1);
    // Sensor stations.
    const stations=[[-9.2,2.1,'Potenciómetro de suelo · GPIO34','soil'],[-1,4,'Sensor DHT22 · GPIO4','dht'],[4.5,-4.5,'Fotorresistencia · GPIO35','light']];
    for(const [x,z,text,id] of stations){this.cylinder(.04,.06,2,'#979d82',x,1.1,z,undefined,6);this.box(.46,.7,.22,id==='soil'?'#6d8e50':'#e7e9da',x,2,z);if(id==='dht')for(let j=0;j<4;j++)this.box(.35,.025,.015,'#a1ae98',x,1.8+j*.12,z+.12);this.label(text,[x,2.85,z],'',id);this.pipe([[x,.32,z],[x,.3,13],[-15,.3,13],[-15,1.8,1]],'#9b9263',.025);this.route([[x,2.1,z],[x-2,4,z],[-15,4,1],[-15,2.7,1]],'#a8bf6d','sensor');}
    this.label('Sensor ultrasónico HC-SR04',[13.3,5.6,-6.3],'','tank');this.label('ESP32 · control local',[-15,3.8,1],'','esp32');this.label('Bomba de riego',[11.1,1.9,-2],'amber','pump');
    this.route([[13.3,4.9,-6.3],[5,6,-5],[-8,5,0],[-15,2.8,1]],'#a8bf6d','sensor');
    // Wi-Fi gateway and MQTT service exist as an explanatory overlay.
    this.networkGroup=new THREE.Group();this.scene.add(this.networkGroup);
    this.box(1.2,.25,.65,'#dfe5db',-14,5.2,-10.8);for(const x of [-14.4,-13.6])this.cylinder(.025,.025,.9,'#567464',x,5.7,-10.8);
    for(let i=0;i<3;i++){const arc=new THREE.Mesh(new THREE.TorusGeometry(.55+i*.37,.025,5,24,Math.PI),this.mat('#82adb1'));arc.position.set(-14,6,-10.8);this.scene.add(arc);}
    this.label('Wi-Fi · router',[-14,7.1,-10.8],'blue','router');
    for(const [x,y,z,r] of [[0,0,0,1.25],[-1.3,-.3,0,.9],[1.2,-.25,0,.95],[.3,.6,0,.8]])this.ball(r,'#f1f5ec',16+x,9.6+y,-10+z,this.networkGroup,[1,.8,.7]);
    this.label('Internet · broker MQTT',[16,11,-10],'blue','broker');
    this.route([[-15,3,1],[-16,8,-3],[-14,7,-10.8]],'#77aebb','wifi');
    this.route([[-14,7,-10.8],[-2,14,-14],[9,13,-13],[16,9.7,-10]],'#77aebb','mqtt');
    // Browser representation on a small plinth.
    this.box(3.4,.15,2.2,'#bac4ad',15.8,.2,7.5);this.box(.16,1.9,.16,'#8f9d88',15.8,1.2,7.5);this.box(2.8,1.65,.18,'#536d5e',15.8,2.7,7.5);this.box(2.52,1.38,.04,'#c8e2d6',15.8,2.7,7.62);
    for(let i=0;i<4;i++)this.box(.35,.25+i*.18,.04,['#749b61','#83ac79','#8ebc92','#699a8d'][i],14.99+i*.52,2.4+i*.08,7.66);
    this.label('Gemelo digital · WSS',[15.8,4,7.5],'blue','viewer');this.route([[16,9.7,-10],[21,9,-3],[20,7,4],[15.8,3,7.5]],'#77aebb','mqtt');
    // Auxiliary light pole near the maintenance path.
    this.cylinder(.065,.1,4.6,'#83917c',7.2,2.4,8.5);this.box(1.1,.15,.65,'#a1ac91',7.2,4.7,8.5);this.lamp=this.box(.9,.05,.5,'#e8e6bd',7.2,4.6,8.5);this.lamp.material.emissive=new THREE.Color('#ffe3a1');this.lampLight=new THREE.PointLight('#ffda9c',0,10);this.lampLight.position.set(7.2,4.2,8.5);this.scene.add(this.lampLight);
    this.focal={sensors:[-4,1,3],soil:[-9.2,1,2.1],dht:[-1,2,4],light:[4.5,2,-4.5],tank:[13.3,2,-6.3],esp32:[-15,2.5,1],router:[-14,5,-10.8],broker:[16,9.6,-10],viewer:[15.8,2.7,7.5],pump:[11,1,-2]};
    document.querySelector('#scene-loading')?.remove();
  }
  resize(){const w=this.container.clientWidth,h=this.container.clientHeight;if(!w||!h)return;this.renderer.setSize(w,h);this.camera.aspect=w/h;this.camera.updateProjectionMatrix();}
  setState(state,live=false,stale=false){this.state=state;this.live=live;this.stale=stale;const level=THREE.MathUtils.clamp((state.tank??0)/100,.001,1);this.tankWater.scale.y=level;this.tankWater.position.y=.31+3.9*level/2;this.lamp.material.emissiveIntensity=state.lamp&&!stale?1.1:0;this.lampLight.intensity=state.lamp&&!stale?12:0;}
  packet(source="mqtt"){if(source==="mqtt")this.lastNetworkPacket=performance.now();this.lastPacket=performance.now();}
  setView(mode){this.networkView=mode==='network';this.controls.target.set(0,this.networkView?3:1,-1);const target=this.networkView?new THREE.Vector3(43,39,47):new THREE.Vector3(41,38,45);this.camera.position.copy(target);}
  focus(id){const p=this.focal[id];if(!p)return;this.controls.target.set(...p);this.camera.position.set(p[0]+15,p[1]+13,p[2]+18);}
  reset(){this.setView(this.networkView?'network':'field');}
  toggleLabels(){this.labelsVisible=!this.labelsVisible;}
  animate(){requestAnimationFrame(()=>this.animate());if(document.hidden)return;if(document.querySelector("dialog[open]"))return;if(this.software&&this.lastRender&&performance.now()-this.lastRender<500)return;this.lastRender=performance.now();const t=this.clock.getElapsedTime();this.controls.update();const packetActive=performance.now()-this.lastPacket<2100&&!this.stale;
    for(const r of this.routes){r.line.visible=this.networkView;r.line.material.opacity=this.networkView?.55:.15;}
    for(const p of this.pulses){p.dot.visible=packetActive&&(p.type==='sensor'||performance.now()-(this.lastNetworkPacket||0)<2100)&&(this.networkView||p.type!=='sensor');if(p.dot.visible)p.dot.position.copy(p.curve.getPointAt((t*.22+p.offset)%1));}
    for(const d of this.drops){d.mesh.visible=!!this.state.pump&&!this.stale;if(d.mesh.visible){const phase=(t*1.5+d.phase)%1;d.mesh.position.set(d.x+.14+Math.sin(phase*6)*.04,.4-phase*.25,d.z);}}
    if(this.state.pump&&!this.stale)this.pumpRotor.rotation.z=t*5;
    this.controllerLed.material.emissiveIntensity=packetActive?.7+.3*Math.sin(t*9):.15;
    for(const label of this.labels){const v=label.point.clone().project(this.camera);label.el.style.display=this.labelsVisible&&v.z<1&&(this.networkView||['soil','tank','esp32','pump'].includes(label.id))?'block':'none';label.el.style.left=(v.x*.5+.5)*this.container.clientWidth+'px';label.el.style.top=(-v.y*.5+.5)*this.container.clientHeight+'px';}
    this.renderer.render(this.scene,this.camera);
  }
}