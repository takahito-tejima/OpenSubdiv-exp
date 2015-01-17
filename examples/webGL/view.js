var vTexture = null;
var nTexture = null;
var mTexture = null;
var rx = 0;
var ry = 0;
var dolly = 5;
var center = [0, 0, 0];
var fov = 60;
var maxlevel = 1;
var time = 0;
var model = null;
var deform = false;
var drawHull = true;
var uvMapping = false;
var prevTime = 0;
var fps = 0;
var uvimage = new Image();
var uvtex = null;
var program = null;
var interval = null;
var framebuffer = null;

var displayMode = 1;


var tessFactor = 1;

function windowEvent(){
    if (window.event) 
        return window.event;
    var caller = arguments.callee.caller;
    while (caller) {
        var ob = caller.arguments[0];
        if (ob && ob.constructor == MouseEvent)
            return ob;
        caller = caller.caller;
    }
    return null;
}

function getMousePosition(){
    var event = windowEvent();
    var canvas = $("#main").get(0);
    canvasOffsetX = canvas.offsetLeft;
    canvasOffsetY = canvas.offsetTop;
    var x = event.pageX - canvasOffsetX;
    var y = event.pageY - canvasOffsetY;
    return vec3.create([x, y, 0]);
}

function buildProgram(vertexShader, fragmentShader)
{
    var define = "";
    if (uvMapping) define += "#define USE_UV_MAP\n";
    var util = $('#shaderutil').text();

    var program = gl.createProgram();
    var vshader = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(vshader, define+util+$(vertexShader).text());
    gl.compileShader(vshader);
    if (!gl.getShaderParameter(vshader, gl.COMPILE_STATUS))
        alert(gl.getShaderInfoLog(vshader));
    var fshader = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fshader, define+$(fragmentShader).text());
    gl.compileShader(fshader);
    if (!gl.getShaderParameter(fshader, gl.COMPILE_STATUS))
        alert(gl.getShaderInfoLog(fshader));
    gl.attachShader(program, vshader);
    gl.attachShader(program, fshader);
    //gl.bindAttribLocation(program, 0, "vertexID");

    gl.bindAttribLocation(program, 0, "position");
    gl.bindAttribLocation(program, 1, "inNormal");
    gl.bindAttribLocation(program, 2, "inUV");

    gl.linkProgram(program)
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) 
        alert(gl.getProgramInfoLog(program));
    return program;
}

function createTextureBuffer2(data, format, reso)
{
    if(format == gl.LUMINANCE) {
	data.length = reso*reso;
    }else if(format == gl.LUMINANCE_ALPHA)
	data.length = reso*reso*2;
    else if(format == gl.RGB)
	data.length = reso*reso*3;
    else if(format == gl.RGBA)
	data.length = reso*reso*4;

    var texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, true);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    gl.texImage2D(gl.TEXTURE_2D, 0, format, reso, reso,
		  0, format, gl.FLOAT, data);

    return texture;
}

function createTextureBuffer(data, format)
{
    var texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, true);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    if (data.length > reso*reso) {
	console.log("Data too large ", data.length);
    }
    
    var d = new Array();
    for(var j=0; j<data.length; j++){
	d.push(data[j]);
    }
    if(format == gl.LUMINANCE) 
	d.length = reso*reso;
    else if(format == gl.LUMINANCE_ALPHA)
	d.length = reso*reso*2;
    else if(format == gl.RGB)
	d.length = reso*reso*3;
    else if(format == gl.RGBA)
	d.length = reso*reso*4;
    gl.texImage2D(gl.TEXTURE_2D, 0, format, reso, reso,
		  0, format, gl.FLOAT, new Float32Array(d));
    return texture;
}

function dumpFrameBuffer()
{
    var buffer = new ArrayBuffer(reso*reso*4);
    var pixels = new Uint8Array(buffer);
    gl.readPixels(0, 0, reso, reso, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    console.log(pixels);
}

function initialize(){

    var reso = 512;
    // create vertex array
    var vertexIDs = new Array();
    for(i = 0; i <reso*reso; i++){
	vertexIDs.push(i);
    }
    numVertex = vertexIDs.length;
    vbuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vbuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertexIDs), gl.STATIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);

    buildMainProgram();

    cageProgram = buildProgram("#cageVS", "#cageFS");
    cageProgram.mvpMatrix = gl.getUniformLocation(cageProgram, "mvpMatrix");

//    progFace = buildProgram('#faceKernel', '#kfshader');
//    progEdge = buildProgram('#edgeKernel', '#kfshader');
//    progVertexA = buildProgram('#vertexKernelA', '#kfshader');
//    progVertexB = buildProgram('#vertexKernelB', '#kfshader');

}

function buildMainProgram()
{
    if (program == null)
	gl.deleteProgram(program);
    program = buildProgram('#vshader', '#fshader');
    program.mvpMatrix = gl.getUniformLocation(program, "mvpMatrix");
    program.modelViewMatrix = gl.getUniformLocation(program, "modelViewMatrix");
    program.projMatrix = gl.getUniformLocation(program, "projMatrix");

    program.displayMode = gl.getUniformLocation(program, "displayMode");
}


function deleteModel(data) {
    if(data == null) return;

    for(var i=0; i<data.maxLevel; i++){
	gl.deleteBuffer(data.level[i].ibLines);
	gl.deleteBuffer(data.level[i].ibTriangles);
    }
    gl.deleteBuffer(data.ibHulls);
    gl.deleteTexture(data.texF_IT);
    gl.deleteTexture(data.texF_ITa);
    gl.deleteTexture(data.texE_IT);
    gl.deleteTexture(data.texV_IT);
    gl.deleteTexture(data.texV_ITa1);
    gl.deleteTexture(data.texV_ITa2);
    gl.deleteTexture(data.texE_W);
    gl.deleteTexture(data.texV_W);
}

function fit() {
    if (model == null) return;

    var n = model.cageVerts.length;
    var min = [model.cageVerts[0], model.cageVerts[1], model.cageVerts[2]];
    var max = [min[0], min[1], min[2]];
    for (i = 0; i < n; i+= 3) {
        var p = [model.cageVerts[i], model.cageVerts[i+1], model.cageVerts[i+2]];
        for (var j = 0; j < 3; ++j) {
            min[j] = (min[j] < p[j]) ? min[j] : p[j];
            max[j] = (max[j] > p[j]) ? max[j] : p[j];
        }
    }
    var size = [max[0]-min[0], max[1]-min[1], max[2]-min[2]];
    var diag = Math.sqrt(size[0]*size[0] + size[1]*size[1] + size[2]*size[2]);

    dolly = diag*0.8;

    center = [(max[0]+min[0])*0.5, (max[1]+min[1])*0.5, (max[2]+min[2])*0.5];
}

function setModel(data) {


    if (data == null) return;

    //console.log(data);

    // XXX: release buffers!
    model = {};
    model.patchVerts    = [];

    // control cage
    model.animVerts   = new Float32Array(3*data.points.length)
    model.cageVerts   = new Float32Array(3*data.points.length)
    for (i = 0; i < data.points.length; i++) {
        model.cageVerts[i*3+0] = data.points[i][0];
        model.cageVerts[i*3+1] = data.points[i][1];
        model.cageVerts[i*3+2] = data.points[i][2];
        model.animVerts[i*3+0] = data.points[i][0];
        model.animVerts[i*3+1] = data.points[i][1];
        model.animVerts[i*3+2] = data.points[i][2];
        model.patchVerts[i] = data.points[i];
    }
    model.cageLines   = new Int16Array(data.hull.length)
    for (i = 0; i < data.hull.length; i++) {
        model.cageLines[i] = data.hull[i];
    }

    model.hullVerts = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, model.hullVerts);
    gl.bufferData(gl.ARRAY_BUFFER, model.animVerts, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);

    var ibuffer = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibuffer);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, model.cageLines, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);

    model.hullIndices = ibuffer;

    // patch vertices
    var nPoints = data.stencilIndices.length;
    model.stencilIndices = data.stencilIndices;
    model.stencilWeights = data.stencilWeights;

    // patch indices
    model.tessvbo = gl.createBuffer();
    model.patches = data.patches;
    model.patchParams = data.patchParams;

    model.maxValence = data.maxValence;
    model.valenceTable = data.vertexValences;
    model.quadOffsets = data.quadOffsets;

    fit();

    updateGeom();
}

function updateGeom() {
    refine();
    tessellate();
    redraw();
}

function animate() {
    var r = Math.cos(time);
    for (var i = 0; i < model.cageVerts.length; i += 3) {
        var x = model.cageVerts[i+0];
        var y = model.cageVerts[i+1];
        var z = model.cageVerts[i+2];
        model.animVerts[i+0] = x * Math.cos(r*y) + z * Math.sin(r*y);
        model.animVerts[i+1] = y;
        model.animVerts[i+2] = - x * Math.sin(r*y) + z * Math.cos(r*y);
        model.patchVerts[i/3] = [model.animVerts[i+0],
                               model.animVerts[i+1],
                               model.animVerts[i+2]];
    }
    time = time + 0.1;

    gl.bindBuffer(gl.ARRAY_BUFFER, model.hullVerts);
    gl.bufferData(gl.ARRAY_BUFFER, model.animVerts, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);
}

function refine() {
    if (model == null) return;

    for (i = 0; i < model.stencilIndices.length; i++) {
        // apply stencil
        var x = 0, y = 0, z = 0;
        var size = model.stencilIndices[i].length;
        for (j = 0; j < size; j++) {
            var vindex = model.stencilIndices[i][j];
            var weight = model.stencilWeights[i][j];
            x += model.animVerts[vindex*3+0] * weight;
            y += model.animVerts[vindex*3+1] * weight;
            z += model.animVerts[vindex*3+2] * weight;
        }
        model.patchVerts[i + model.cageVerts.length/3] = [x,y,z];
    }
}

function evalCubicBezier(u, B, BU) {
    var t = u;
    var s = 1 - u;

    var A0 = s * s;
    var A1 = 2 * s * t;
    var A2 = t * t;

    B[0] = s * A0;
    B[1] = t * A0 + s * A1;
    B[2] = t * A1 + s * A2;
    B[3] = t * A2;
    BU[0] =    - A0;
    BU[1] = A0 - A1;
    BU[2] = A1 - A2;
    BU[3] = A2;
}

function evalCubicBSpline(u, B, BU) {
    var t = u;
    var s = 1 - u;
    var A0 =                      s * (0.5 * s);
    var A1 = t * (s + 0.5 * t) + s * (0.5 * s + t);
    var A2 = t * (    0.5 * t);
    B[0] =                                     1/3.0 * s                * A0;
    B[1] = (2/3.0 * s +           t) * A0 + (2/3.0 * s + 1/3.0 * t) * A1;
    B[2] = (1/3.0 * s + 2/3.0 * t) * A1 + (          s + 2/3.0 * t) * A2;
    B[3] =                1/3.0 * t  * A2;
    BU[0] =    - A0;
    BU[1] = A0 - A1;
    BU[2] = A1 - A2;
    BU[3] = A2;
}

function csf(n, j)
{
    if (j%2 == 0) {
        return Math.cos((2.0 * Math.PI * ((j-0)/2.0))/(n + 3.0));
    } else {
        return Math.sin((2.0 * Math.PI * ((j-1)/2.0))/(n + 3.0));
    }
}

function evalGregory(indices, quadOffset, u, v) {

    var valences = [];
    var maxValence = model.maxValence;

    var ef_small = [0.813008, 0.500000, 0.363636, 0.287505,
                    0.238692, 0.204549, 0.179211];

    var r = [], rp, e0 = [], e1 = [];
    //float  *r  = (float*)alloca((maxValence+2)*4*length*sizeof(float)), *rp,
        //*e0 = r + maxValence*4*length,
        //*e1 = e0 + 4*length;

    //for (var i = 0; i < (maxValence+2)*4; ++i) r[i] = [0,0,0];

    var f = [], pos, opos = [];
    var valenceTable = model.valenceTable;
    var quadOffsets = model.quadOffsets;
    var verts = model.patchVerts;

    for (var vid=0; vid < 4; ++vid) {
        var vertexID = indices[vid];
        var valenceTableOffset = vertexID * (2*maxValence+1);
        var valence = valenceTable[valenceTableOffset];
        valences[vid] = valence;

        // read vertexID
        pos = verts[vertexID];

        //rp=r+vid*maxValence*length;
        rp = vid*maxValence;
        //var vofs = vid;
        opos[vid] = [0,0,0];

        for (var i=0; i<valence; ++i) {
            var im = (i+valence-1)%valence;
            var ip = (i+1)%valence;

            var idx_neighbor   = valenceTable[valenceTableOffset + 2*i  + 0 + 1];
            var idx_diagonal   = valenceTable[valenceTableOffset + 2*i  + 1 + 1];
            var idx_neighbor_p = valenceTable[valenceTableOffset + 2*ip + 0 + 1];
            var idx_neighbor_m = valenceTable[valenceTableOffset + 2*im + 0 + 1];
            var idx_diagonal_m = valenceTable[valenceTableOffset + 2*im + 1 + 1];

            var neighbor   = verts[idx_neighbor];
            var diagonal   = verts[idx_diagonal];
            var neighbor_p = verts[idx_neighbor_p];
            var neighbor_m = verts[idx_neighbor_m];
            var diagonal_m = verts[idx_diagonal_m];

            //float  *fp = f+i*3;
            f[i] = [0,0,0];
            //r[rp] = [0,0,0];
            r[rp+i] = [0,0,0];
            for (var k=0; k<3; ++k) {
                f[i][k] = (pos[k]*valence
                           + (neighbor_p[k]+neighbor[k])*2.0
                           + diagonal[k])/(valence+5.0);

                opos[vid][k] += f[i][k];
                r[rp+i][k] =(neighbor_p[k]-neighbor_m[k])/3.0
                    + (diagonal[k]-diagonal_m[k])/6.0;
            }
        }

        // average opos
        for (var k=0; k<3; ++k) {
            opos[vid][k] /= valence;
        }

        // compute e0, e1
        e0[vid] = [0,0,0];
        e1[vid] = [0,0,0];
        for (var i=0; i<valence; ++i) {
            var im = (i+valence-1)%valence;
            for (var k=0; k<3; ++k) {
                var e = 0.5*(f[i][k] + f[im][k]);
                e0[vid][k] += csf(valence-3, 2*i) * e;
                e1[vid][k] += csf(valence-3, 2*i+1) * e;
            }
        }
        for (var k=0; k<3; ++k) {
            e0[vid][k] *= ef_small[valence-3];
            e1[vid][k] *= ef_small[valence-3];
        }
    }

    var Ep = [[],[],[],[]];
    var Em = [[],[],[],[]];
    var Fp = [[],[],[],[]];
    var Fm = [[],[],[],[]];

    for (var vid=0; vid<4; ++vid) {
        var ip = (vid+1)%4;
        var im = (vid+3)%4;
        var n = valences[vid];

        var start = quadOffsets[quadOffset + vid] & 0x00ff;
        var prev = (quadOffsets[quadOffset + vid] & 0xff00) / 256;

        for (var k=0; k<3; ++k) {
            Ep[vid][k] = opos[vid][k] + e0[vid][k] * csf(n-3, 2*start)
                + e1[vid][k]*csf(n-3, 2*start +1);
            Em[vid][k] = opos[vid][k] + e0[vid][k] * csf(n-3, 2*prev )
                + e1[vid][k]*csf(n-3, 2*prev + 1);
        }

        var np = valences[ip];
        var nm = valences[im];

        var prev_p = (quadOffsets[quadOffset + ip] & 0xff00) / 256;
        var start_m = quadOffsets[quadOffset + im] & 0x00ff;

        var Em_ip = [0,0,0];
        var Ep_im = [0,0,0];

        for (var k=0; k<3; ++k) {
            Em_ip[k] = opos[ip][k] + e0[ip][k]*csf(np-3, 2*prev_p)
                + e1[ip][k]*csf(np-3, 2*prev_p+1);
            Ep_im[k] = opos[im][k] + e0[im][k]*csf(nm-3, 2*start_m)
                + e1[im][k]*csf(nm-3, 2*start_m+1);
        }

        var s1 = 3.0 - 2.0*csf(n-3,2)-csf(np-3,2);
        var s2 = 2.0 * csf(n-3,2);
        var s3 = 3.0 - 2.0*Math.cos(2.0*Math.PI/n) - Math.cos(2.0*Math.PI/nm);

        rp = vid*maxValence;
        for (var k=0; k<3; ++k) {
            Fp[vid][k] = (csf(np-3,2)*opos[vid][k]
                          + s1*Ep[vid][k] + s2*Em_ip[k] + r[rp+start][k])/3.0;
            Fm[vid][k] = (csf(nm-3,2)*opos[vid][k]
                          + s3*Em[vid][k] + s2*Ep_im[k] - r[rp+prev][k])/3.0;
        }
    }

    var p = [];
    for (var i=0; i<4; ++i) {
        p[i*5+0] = opos[i];
        p[i*5+1] =   Ep[i];
        p[i*5+2] =   Em[i];
        p[i*5+3] =   Fp[i];
        p[i*5+4] =   Fm[i];
    }

    var U = 1-u, V=1-v;
    var d11 = u+v; if(u+v==0.0) d11 = 1.0;
    var d12 = U+v; if(U+v==0.0) d12 = 1.0;
    var d21 = u+V; if(u+V==0.0) d21 = 1.0;
    var d22 = U+V; if(U+V==0.0) d22 = 1.0;

    var q = [];
    for (var i= 0; i<16; ++i) q[i] = [0,0,0];
    for (var k=0; k<3; ++k) {
        q[ 5][k] = (u*p[ 3][k] + v*p[ 4][k])/d11;
        q[ 6][k] = (U*p[ 9][k] + v*p[ 8][k])/d12;
        q[ 9][k] = (u*p[19][k] + V*p[18][k])/d21;
        q[10][k] = (U*p[13][k] + V*p[14][k])/d22;

        q[ 0][k] = p[ 0][k];
        q[ 1][k] = p[ 1][k];
        q[ 2][k] = p[ 7][k];
        q[ 3][k] = p[ 5][k];
        q[ 4][k] = p[ 2][k];
        q[ 7][k] = p[ 6][k];
        q[ 8][k] = p[16][k];
        q[11][k] = p[12][k];
        q[12][k] = p[15][k];
        q[13][k] = p[17][k];
        q[14][k] = p[11][k];
        q[15][k] = p[10][k];
    }

    // bezier evaluation

    B = [0, 0, 0, 0];
    D = [0, 0, 0, 0];
    BU = [[0,0,0], [0,0,0], [0,0,0], [0,0,0]];
    DU = [[0,0,0], [0,0,0], [0,0,0], [0,0,0]];

    evalCubicBezier(u, B, D);
    for (var i = 0; i < 4; i++) {
        for(var j=0;j<4;j++){
            for(var k=0; k<3; k++){
                BU[i][k] += q[i+j*4][k] * B[j];
                DU[i][k] += q[i+j*4][k] * D[j];
            }
        }
    }
    evalCubicBezier(v, B, D);

    Q = [0, 0, 0];
    QU = [0, 0, 0];
    QV = [0, 0, 0];
    for(var i=0;i<4; i++){
        for (var k=0; k<3; k++) {
            Q[k] += BU[i][k] * B[i];
            QU[k] += DU[i][k] * B[i];
            QV[k] += BU[i][k] * D[i];
        }
    }
    N = [QU[1]*QV[2]-QU[2]*QV[1],
         QU[2]*QV[0]-QU[0]*QV[2],
         QU[0]*QV[1]-QU[1]*QV[0]];

    return [Q[0], Q[1], Q[2], -N[0], -N[1], -N[2]];
}

function evalBSpline(indices, u, v) {

    B = [0, 0, 0, 0];
    D = [0, 0, 0, 0];
    BU = [[0,0,0], [0,0,0], [0,0,0], [0,0,0]];
    DU = [[0,0,0], [0,0,0], [0,0,0], [0,0,0]];

    evalCubicBSpline(u, B, D);
    for (var i = 0; i < 4; i++) {
        for(var j=0;j<4;j++){
            var vid = indices[i+j*4];
            for(var k=0; k<3; k++){
                BU[i][k] += model.patchVerts[vid][k] * B[j];
                DU[i][k] += model.patchVerts[vid][k] * D[j];
            }
        }
    }
    evalCubicBSpline(v, B, D);

    Q = [0, 0, 0];
    QU = [0, 0, 0];
    QV = [0, 0, 0];
    for(var i=0;i<4; i++){
        for (var k=0; k<3; k++) {
            Q[k] += BU[i][k] * B[i];
            QU[k] += DU[i][k] * B[i];
            QV[k] += BU[i][k] * D[i];
        }
    }
    N = [QU[1]*QV[2]-QU[2]*QV[1],
         QU[2]*QV[0]-QU[0]*QV[2],
         QU[0]*QV[1]-QU[1]*QV[0]];
    return [Q[0], Q[1], Q[2], -N[0], -N[1], -N[2]];
}

function appendVBO(points, indices)
{
    var batch = {}

    var pdata = new Float32Array(points.length);
    for (i = 0; i < points.length; i++) {
        pdata[i] = points[i];
    }
    batch.nPoints = pdata.length/3;
    var idata = new Uint16Array(indices.length);
    for (i = 0; i < indices.length; i++) {
        idata[i] = indices[i];
    }
    batch.nTris = idata.length/3;

    batch.vbo = gl.createBuffer();
    batch.ibo = gl.createBuffer();

    gl.bindBuffer(gl.ARRAY_BUFFER, batch.vbo);
    gl.bufferData(gl.ARRAY_BUFFER, pdata, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);

    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, batch.ibo);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, idata, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);

    model.batches.push(batch);
}

function tessellate() {
    if (model == null) return;

    model.batches = []

    var points = [];
    var indices = [];
    var vid = 0;
    for (var i = 0; i < model.patches.length; i++) {
        var ncp = model.patches[i].length;
        if (ncp == 9 || ncp == 12) continue;  // boundary, corner patch

        if (i >= model.patchParams.length) continue;
        var p = model.patchParams[i];
        if (p ==null) continue;

        var level = 1 + tessFactor - p[0];
        if (level < 0) level = 0;
        var div = (1 << level) + 1;
        var vbegin = vid;
        for (iu = 0; iu < div; iu++) {
            for (iv = 0; iv < div; iv++) {
                var u = iu/(div-1);
                var v = iv/(div-1);
                if (ncp == 4) {
                    var quadOffset = i*4;
                    pn = evalGregory(model.patches[i], quadOffset, u, v);
                } else {
                    pn = evalBSpline(model.patches[i], u, v);
                }
                points.push(pn[0]);
                points.push(pn[1]);
                points.push(pn[2]);
                points.push(pn[3]);
                points.push(pn[4]);
                points.push(pn[5]);
                points.push(u);
                points.push(v);
                points.push(iu);
                points.push(iv);
                if (iu != 0 && iv != 0) {
                    indices.push(vid);
                    indices.push(vid-div);
                    indices.push(vid-div-1);
                    indices.push(vid-1);
                    indices.push(vid-div-1);
                    indices.push(vid);
                }
                ++vid;
            }
        }
        // if it reached to 64K vertices, move to next batch
        if (vid > 60000) {
            appendVBO(points, indices);
            points = [];
            indices = [];
            vid = 0;
        }
    }

    // residual
    appendVBO(points, indices);
}

function syncbuffer()
{
//    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, vTexture, 0);
    gl.flush();
//    dumpFrameBuffer();
}

function subdivide(targetTexture) {

    return;

    gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, targetTexture, 0);

    var reso = model.resolution;
    gl.viewport(0, 0, reso, reso);
    gl.disable(gl.DEPTH_TEST);

    if(model.levels == undefined) return;

    gl.enableVertexAttribArray(0);
    gl.bindBuffer(gl.ARRAY_BUFFER, vbuffer);
    gl.vertexAttribPointer(0, 1, gl.FLOAT, false, 0, 0);
}

function idle() {

    if (model == null) return;
    animate();
    updateGeom();
}

function redraw() {

    if (model == null) return;

//    gl.bindFramebuffer(gl.FRAMEBUFFER, null);

    
    //gl.clearColor(0, 0, 0, 0);
    gl.clearColor(.1, .1, .2, 1);
    //gl.clearDepth(1000);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.enable(gl.DEPTH_TEST);
    gl.disable(gl.BLEND);
    gl.depthFunc(gl.LEQUAL);

    
    var canvas = $('#main');
    var w = canvas.width();
    var h = canvas.height();
    var aspect = w / h;
    gl.viewport(0, 0, w, h);
    
    var proj = mat4.create();
    mat4.identity(proj);
    mat4.perspective(fov, aspect, 0.1, 1000.0, proj);
    
    var modelView = mat4.create();
    mat4.identity(modelView);
    mat4.translate(modelView, [0, 0, -dolly], modelView);
    mat4.rotate(modelView, ry*Math.PI*2/360, [1, 0, 0], modelView);
    mat4.rotate(modelView, rx*Math.PI*2/360, [0, 1, 0], modelView);
    mat4.translate(modelView, [-center[0], -center[1], -center[2]], modelView);
    
    var mvpMatrix = mat4.create();
    mat4.multiply(proj, modelView, mvpMatrix);


    if (drawHull) {
        gl.useProgram(cageProgram);
        gl.uniformMatrix4fv(cageProgram.mvpMatrix, false, mvpMatrix);

        gl.bindBuffer(gl.ARRAY_BUFFER, model.hullVerts);
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, model.hullIndices);
        gl.enableVertexAttribArray(0);
        gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 0, 0);

        gl.drawElements(gl.LINES, model.cageLines.length, gl.UNSIGNED_SHORT, 0);
    }

    // ---------------------------
    gl.useProgram(program);
    gl.uniformMatrix4fv(program.modelViewMatrix, false, modelView);
    gl.uniformMatrix4fv(program.projMatrix, false, proj);
    gl.uniformMatrix4fv(program.mvpMatrix, false, mvpMatrix);
    gl.uniform1i(program.displayMode, displayMode);

    var drawTris = 0;
    gl.enableVertexAttribArray(0);
    gl.enableVertexAttribArray(1);
    gl.enableVertexAttribArray(2);
    for (var i = 0; i < model.batches.length; ++i) {
        var batch = model.batches[i];
        gl.bindBuffer(gl.ARRAY_BUFFER, batch.vbo);
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, batch.ibo);
        gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 10*4, 0);   // XYZ
        gl.vertexAttribPointer(1, 3, gl.FLOAT, false, 10*4, 3*4); // normal
        gl.vertexAttribPointer(2, 4, gl.FLOAT, false, 10*4, 6*4); // uv, iuiv

        //gl.drawElements(gl.POINTS, batch.nTris*3, gl.UNSIGNED_SHORT, 0);
        gl.drawElements(gl.TRIANGLES, batch.nTris*3, gl.UNSIGNED_SHORT, 0);

        drawTris += batch.nTris;
    }

    gl.disableVertexAttribArray(0);
    gl.disableVertexAttribArray(1);
    gl.disableVertexAttribArray(2);
/*

    if (drawWire) {
	gl.uniform3f(gl.getUniformLocation(program, "diffuse"), 0, 0, 0);
	gl.uniform3f(gl.getUniformLocation(program, "ambient"), 0.2, 0.2, 0.2);

	gl.bindBuffer(gl.ARRAY_BUFFER, model.levels[maxlevel-1].ibLines);
	gl.vertexAttribPointer(0, 1, gl.FLOAT, false, 0, 0);
	gl.drawArrays(gl.LINES, 0, model.levels[maxlevel-1].nLines);
    }
    if (drawHull) {
	gl.uniform3f(gl.getUniformLocation(program, "diffuse"), 0, 0, 0);
	gl.uniform3f(gl.getUniformLocation(program, "ambient"), 0, 0, 0.5);

	gl.bindBuffer(gl.ARRAY_BUFFER, model.ibHulls);
	gl.vertexAttribPointer(0, 1, gl.FLOAT, false, 0, 0);
	gl.drawArrays(gl.LINES, 0, model.nHulls);
    }
*/
	
    gl.finish();

    var time = Date.now();
    drawTime = time - prevTime;
    prevTime = time;
    //fps = (29 * fps + 1000.0/drawTime)/30.0;
    fps = 1000.0/drawTime;
    $('#fps').text(Math.round(fps));
    $('#triangles').text(drawTris);
}

function loadModel(url)
{
/*
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.responseType = 'arraybuffer';
    
    xhr.onload = function(e) {
	setModelBin(this.response);
	redraw();
    }
    xhr.send();
*/
    var type = "text"
    $.ajax({
	type: "GET",
	url: url,
	responseType:type,
	success: function(data) {
            console.log("Get");
	    setModel(data.model);
	    redraw();
	}
    });
}

$(function(){
    var canvas = $("#main").get(0);
    $.each(["webgl", "experimental-webgl", "webkit-3d", "moz-webgl"], function(i, name){
	try {
	    gl = canvas.getContext(name);
            gl.getExtension('OES_standard_derivatives');

	} 
	catch (e) {
	}
	return !gl;
    });
    if (!gl) {
	alert("WebGL is not supported in this browser!");
	return;
    }
    if(!gl.getExtension('OES_texture_float')){
	alert("requires OES_texture_float extension");
    }
    
    initialize();

    var button = false;
    var prev_position;
    document.onmousemove = function(e){
	var event = windowEvent();
	var p = getMousePosition();
	if (button > 0) {
	    var d = vec3.subtract(p, prev_position, vec3.create());
	    prev_position = p;
	    if (button == 1) {
		rx += d[0];
		ry += d[1];
		if(ry > 90) ry = 90;
		if(ry < -90) ry = -90;
	    }
	    else if(button == 3){
                /*
		fov -= d[0];
		if(fov < 1) fov = 1;
		if(fov > 170) fov = 170;
                */
                dolly -= d[0];
                if (dolly <= 0.001) dolly = 0.001;
	    }
	    redraw();
	}
	return false;
    };
    document.onmousewheel = function(e){
	var event = windowEvent();
	dolly -= event.wheelDelta/200;
	if (dolly < 0.1) dolly = 0.1;
	redraw();
	return false;
    };
    canvas.onmousedown = function(e){
	var event = windowEvent();
	button = event.button + 1;
	prev_position = getMousePosition();
	return false; // keep cursor shape
    };
    document.onmouseup = function(e){
	button = false;
	return false; // prevent context menu
    }
    document.oncontextmenu = function(e){
	return false;
    }

    var modelSelect = $("#modelSelect").get(0);
    modelSelect.onclick = function(e){
	loadModel(modelSelect.value+".json");
	redraw();
    }

    $( "#tessFactorRadio" ).buttonset();
    $( 'input[name="tessFactorRadio"]:radio' ).change(
        function() {
            if (this.id == "tf1") {
                tessFactor = 1;
            } else if (this.id == "tf2") {
                tessFactor = 2;
            } else if (this.id == "tf3") {
                tessFactor = 3;
            } else if (this.id == "tf4") {
                tessFactor = 4;
            } else if (this.id == "tf5") {
                tessFactor = 5;
            } else if (this.id == "tf6") {
                tessFactor = 6;
            }
            updateGeom();
        });

    $( "#radio" ).buttonset();
    $( 'input[name="radio"]:radio' ).change(
        function() {
            if (this.id == "displayShade") {
                displayMode = 0;
            } else if (this.id == "displayWire") {
                displayMode = 1;
            } else if (this.id == "displayNormal") {
                displayMode = 2;
            } else if (this.id == "displayPatch") {
                displayMode = 3;
            }
            redraw();
        });

    $( "#hullCheckbox" ).button().change(
        function(event, ui){
            drawHull = !drawHull;
            redraw();
        });

    $( "#deformCheckbox" ).button().change(
        function(event, ui){
            deform = !deform;
            if (deform) {
                interval = setInterval(idle, 16);
            } else {
                clearInterval(interval);
                interval = null;
                time = 0;
            }
            redraw();
        });

    uvimage.onload = function() {
	uvtex = gl.createTexture();
	gl.bindTexture(gl.TEXTURE_2D, uvtex);
	gl.pixelStorei(gl.UNPACK_ALIGNMENT, true);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.REPEAT);
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, uvimage);

	redraw();
    }


		
    loadModel("cube.json");
});

