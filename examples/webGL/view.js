var vTexture = null;
var nTexture = null;
var mTexture = null;
var rx = 90;
var ry = 0;
var dolly = 5;
var fov = 60;
var maxlevel = 1;
var time = 0;
var model = null;
var deform = false;
var drawWire = true;
var drawHull = true;
var uvMapping = false;
var prevTime = 0;
var fps = 0;
var uvimage = new Image();
var uvtex = null;
var program = null;
var interval = null;
var uvInvalid = true;
var geomInvalid = true;
var framebuffer = null;

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
    gl.bindAttribLocation(program, 0, "vertexID");
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

    gl.bindAttribLocation(program, 0, "inPos");
    gl.bindAttribLocation(program, 1, "inNormal");
    gl.bindAttribLocation(program, 2, "inUV");
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

function getLevelBuffer(header, data, i) {
    var size = header[3+i*14];
    var offset = header[3+i*14+1];
    var resolution = header[3+i*14+2];
    var lb = {};
    lb.buffer = new Float32Array(data.slice(offset, offset+size));
    lb.offset = [];
    lb.resolution = resolution;

    for(j = 0; j < 10; j++){
	lb.offset.push(header[3+i*14+4+j]);
    }
    return lb;
}

function getLevel(header, pos, data) {
    var level = {}
    level.firstOffset = header[pos+0];
    level.numFaceVerts = header[pos+1];
    level.numEdgeVerts = header[pos+2];
    level.numVertexVerts = header[pos+3];
    level.startVB = header[pos+4];
    level.endVB = header[pos+5];
    level.startVA1 = header[pos+6];
    level.endVA1 = header[pos+7];
    level.startVA2 = header[pos+8];
    level.endVA2 = header[pos+9];
    level.triangles = new Float32Array(data.slice(header[pos+11], header[pos+10] + header[pos+11]));
    level.lines = new Float32Array(data.slice(header[pos+13], header[pos+12] + header[pos+13]));
    return level;
}

function setModel(data) {

    console.log(data);
/*
    if (data == null) return;
    deleteModel(model);
    model = {};
    
    header = new Uint32Array(data);
    model.nLevels = header[2];
    model.F_IT = getLevelBuffer(header, data, 0);
    model.F_ITa = getLevelBuffer(header, data, 1);
    model.E_IT = getLevelBuffer(header, data, 2);
    model.V_IT = getLevelBuffer(header, data, 3);
    model.V_ITa1 = getLevelBuffer(header, data, 4);
    model.V_ITa2 = getLevelBuffer(header, data, 5);
    model.E_W = getLevelBuffer(header, data, 6);
    model.V_W = getLevelBuffer(header, data, 7);

    var ofs = 3+14*8;
    model.cageVerts   = new Float32Array(data.slice(header[ofs+1], header[ofs  ]+header[ofs+1]));
    model.cageNormals = new Float32Array(data.slice(header[ofs+3], header[ofs+2]+header[ofs+3]));
    model.cageIndices = new Float32Array(data.slice(header[ofs+5], header[ofs+4]+header[ofs+5]));
    model.resolution = header[ofs+6];

    model.levels = [];
    for(i=0; i<model.nLevels; i++){
	var level = getLevel(header, ofs+14+14*i, data);

	var ibuffer = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, ibuffer);
	gl.bufferData(gl.ARRAY_BUFFER, level.lines, gl.STATIC_DRAW);
	level.ibLines = ibuffer;
	level.nLines = level.lines.length;

	ibuffer = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, ibuffer);
	gl.bufferData(gl.ARRAY_BUFFER, level.triangles, gl.STATIC_DRAW);
	level.ibTriangles = ibuffer;
	level.nTriangles = level.triangles.length;
    
	model.levels.push(level);
    }
*/
    model = {};

    model.patchVerts    = [];

    // control cage
    model.cageVerts   = new Float32Array(3*data.points.length)
    for (i = 0; i < data.points.length; i++) {
        model.cageVerts[i*3+0] = data.points[i][0];
        model.cageVerts[i*3+1] = data.points[i][1];
        model.cageVerts[i*3+2] = data.points[i][2];
        model.patchVerts[i] = data.points[i];
    }
    model.cageLines   = new Int16Array(data.hull.length)
    for (i = 0; i < data.hull.length; i++) {
        model.cageLines[i] = data.hull[i];
    }

    var vbuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vbuffer);
    gl.bufferData(gl.ARRAY_BUFFER, model.cageVerts, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);

    var ibuffer = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibuffer);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, model.cageLines, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);

    model.hullVerts = vbuffer;
    model.hullIndices = ibuffer;

    // patch vertices
    var nPoints = data.stencilIndices.length;
//    model.nPoints = nPoints;

    model.vbo = gl.createBuffer();
    model.ibo = gl.createBuffer();
    model.stencilIndices = data.stencilIndices;
    model.stencilWeights = data.stencilWeights;

    // patch indices
    model.tessvbo = gl.createBuffer();
    model.patches = data.patches;

    refine();
    tessellate();
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
            x += model.cageVerts[vindex*3+0] * weight;
            y += model.cageVerts[vindex*3+1] * weight;
            z += model.cageVerts[vindex*3+2] * weight;
        }
        model.patchVerts[i + model.cageVerts.length/3] = [x,y,z];
    }
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
    return [Q[0], Q[1], Q[2], N[0], N[1], N[2]];
}

function tessellate() {
    if (model == null) return;

    var points = [];
    var indices = [];
    var vid = 0;
    for (var i = 0; i < model.patches.length; i++) {
        if (model.patches[i].length != 16) continue;
        var div = 2;
        var vbegin = vid;
        for (iu = 0; iu < div; iu++) {
            for (iv = 0; iv < div; iv++) {
                var u = iu/(div-1);
                var v = iv/(div-1);
                pn = evalBSpline(model.patches[i], u, v);
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
    }

    var pdata = new Float32Array(points.length);
    for (i = 0; i < points.length; i++) {
        pdata[i] = points[i];
    }
    model.nPoints = pdata.length/3;
    var idata = new Int16Array(indices.length);
    for (i = 0; i < indices.length; i++) {
        idata[i] = indices[i];
    }
    model.nTris = idata.length/3;

    gl.bindBuffer(gl.ARRAY_BUFFER, model.vbo);
    gl.bufferData(gl.ARRAY_BUFFER, pdata, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);

    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, model.ibo);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, idata, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);

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

    if(model == null) return;
    if(maxlevel > model.nLevels) {
	maxlevel = model.nLevels;
    }
    if(deform) {
	geomInvalid = true;
    }
    redraw();
}

var c = 0.1


function redraw() {

    if(model == null) return;
/*
    if (geomInvalid) {
	updateGeom();
	subdivide(vTexture);
	subdivide(nTexture);
	geomInvalid = false;
    }
*/
    if (uvMapping && uvInvalid) {
//	subdivide(mTexture);
	uvInvalid = false;
    }

//    gl.bindFramebuffer(gl.FRAMEBUFFER, null);

    
    //gl.clearColor(0, 0, 0, 0);
    gl.clearColor(1, 1, 1, 1);
    gl.clearDepth(1000);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.enable(gl.DEPTH_TEST);
    gl.disable(gl.BLEND);
    gl.depthFunc(gl.LEQUAL);

    gl.useProgram(program);
    
    var canvas = $('#main');
    var w = canvas.width();
    var h = canvas.height();
    var aspect = w / h;
    gl.viewport(0, 0, w, h);
    
    var proj = mat4.create();
    mat4.identity(proj);
    mat4.perspective(fov, aspect, 0.001, 1000.0, proj);
    
    var modelView = mat4.create();
    mat4.identity(modelView);
    mat4.translate(modelView, [0, 0, -dolly], modelView);
    mat4.rotate(modelView, ry*Math.PI*2/360, [1, 0, 0], modelView);
    mat4.rotate(modelView, rx*Math.PI*2/360, [0, 1, 0], modelView);
    
    var mvpMatrix = mat4.create();
    mat4.multiply(modelView, proj, mvpMatrix);
    
    gl.uniformMatrix4fv(program.modelViewMatrix, false, modelView);
    gl.uniformMatrix4fv(program.projMatrix, false, proj);
    gl.uniformMatrix4fv(program.mvpMatrix, false, mvpMatrix);

    if (drawHull) {
        gl.bindBuffer(gl.ARRAY_BUFFER, model.hullVerts);
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, model.hullIndices);
        gl.enableVertexAttribArray(0);
        gl.disableVertexAttribArray(1);
        gl.disableVertexAttribArray(2);
        gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 0, 0);

        gl.drawElements(gl.LINES, model.cageLines.length, gl.UNSIGNED_SHORT, 0);
    }

    // ---------------------------
    gl.bindBuffer(gl.ARRAY_BUFFER, model.vbo);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, model.ibo);
    gl.enableVertexAttribArray(0);
    gl.enableVertexAttribArray(1);
    gl.enableVertexAttribArray(2);
    gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 10*4, 0);   // XYZ
    gl.vertexAttribPointer(1, 3, gl.FLOAT, false, 10*4, 3*4); // normal
    gl.vertexAttribPointer(2, 4, gl.FLOAT, false, 10*4, 6*4); // uv, iuiv

    //gl.drawArrays(gl.POINTS, 0, model.nPoints);
    gl.drawElements(gl.TRIANGLES, model.nTris*3, gl.UNSIGNED_SHORT, 0);

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
    
    drawTime = Date.now() - prevTime;
    prevTime = Date.now();
    fps = (29 * fps + 1000.0/drawTime)/30.0;
    $('#fps').text(Math.round(fps));
    $('#triangles').text(model.nTris);
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

    var levelSelect = $("#levelSelect").get(0);
    levelSelect.onclick = function(e){
	maxlevel = levelSelect.value;
	geomInvalid = true;
	uvInvalid = true;
	redraw();
    }

    var hullCheckbox = $("#hullCheckbox").get(0);
    hullCheckbox.onchange = function(e){
	drawHull = !drawHull;
	redraw();
    }

    var wireCheckbox = $("#wireCheckbox").get(0);
    wireCheckbox.onchange = function(e){
	drawWire = !drawWire;
	redraw();
    }

    var deformCheckbox = $("#deformCheckbox").get(0);
    deformCheckbox.onchange = function(e){
	deform = !deform;
	if (deform) {
	    interval = setInterval(idle, 16);
	} else {
	    clearInterval(interval);
	    interval = null;
	}
	geomInvalid = true;
	redraw();
    }

    var uvCheckbox = $("#uvCheckbox").get(0);
    uvCheckbox.onchange = function(e){
	uvMapping = !uvMapping;
	if (uvMapping && uvtex == null) {
	    uvimage.src = "brick.jpeg";
	}
	buildMainProgram();
	redraw();
    }

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

