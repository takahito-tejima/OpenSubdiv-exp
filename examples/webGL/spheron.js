var image = new Image();
var texture = null;
var rx = 0;
var ry = 0;
var fov = 60;

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
	//	console.log("A", event.button);
	var canvas = $("#main").get(0);
	canvasOffsetX = canvas.offsetLeft;
	canvasOffsetY = canvas.offsetTop;
	var x = event.pageX - canvasOffsetX;
	var y = event.pageY - canvasOffsetY;
	return vec3.create([x, y, 0]);
}

function initialize(){
	var uDiv = 40;
	var vDiv = 10;
	
	var positions = new Array();
	var uvs = new Array();
	for (var u = 0; u <= uDiv; u++) 
	{
		var su = u / uDiv * Math.PI * 2;
		for (var v = -vDiv; v <= vDiv; v++) 
		{
			var sv = v / vDiv * Math.PI / 2;
			positions.push(Math.cos(su) * Math.cos(sv));
			positions.push(Math.sin(sv));
			positions.push(Math.sin(su) * Math.cos(sv));
			uvs.push(u / uDiv);
			uvs.push(0.5 - (v / vDiv) * 0.5);
		}
	}
	
	vbuffers = $.map([positions, uvs], function(data, i){
		var vbuffer = gl.createBuffer();
		gl.bindBuffer(gl.ARRAY_BUFFER, vbuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(data), gl.STATIC_DRAW);
		return vbuffer;
	});
	gl.bindBuffer(gl.ARRAY_BUFFER, null);
	
	// indices
	ibuffer = gl.createBuffer();
	var indices = new Array();
	var stride = vDiv*2+1;
	for (var u = 0; u < uDiv; u++) 
		{
		for (var v = 0; v < 2*vDiv; v++) 
		{
			indices.push(u*stride + v);
			indices.push(u*stride + v + 1);
			indices.push((u+1)*stride + v);
			indices.push(u*stride + v + 1);
			indices.push((u+1)*stride + v);
			indices.push((u+1)*stride + v+1);
		}
	}
	
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibuffer);
	gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Int16Array(indices), gl.STATIC_DRAW);
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);
	
	numIndices = indices.length;
	
	// shaders
	var vshader = gl.createShader(gl.VERTEX_SHADER);
	gl.shaderSource(vshader, $('#vshader').text());
	gl.compileShader(vshader);
	if (!gl.getShaderParameter(vshader, gl.COMPILE_STATUS)) 
		alert(gl.getShaderInfoLog(vshader));
	
	var fshader = gl.createShader(gl.FRAGMENT_SHADER);
	gl.shaderSource(fshader, $('#fshader').text());
	gl.compileShader(fshader);
	if (!gl.getShaderParameter(fshader, gl.COMPILE_STATUS)) 
		alert(gl.getShaderInfoLog(fshader));
	
	program = gl.createProgram();
	gl.attachShader(program, vshader);
	gl.attachShader(program, fshader);
	
	$.each(["position", "uv"], function(i, name){
		gl.bindAttribLocation(program, i, name);
	});
	
	gl.linkProgram(program);
	if (!gl.getProgramParameter(program, gl.LINK_STATUS)) 
		alert(gl.getProgramInfoLog(program));
	
	program.mvpMatrix = gl.getUniformLocation(program, "mvpMatrix");
	program.modelViewMatrix = gl.getUniformLocation(program, "modelViewMatrix");
	program.projMatrix = gl.getUniformLocation(program, "projMatrix");
}

function renderScene(){
	gl.clearColor(0, 0, 0.2, 1);
	gl.clearDepth(1000);
	gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
	gl.enable(gl.DEPTH_TEST);
	gl.disable(gl.BLEND);
	gl.disable(gl.CULL);
	//    gl.disable(gl.TEXTURING);
	//    gl.enable(gl.TEXTURE_2D);
	
	gl.useProgram(program);
	
	var canvas = $('#main');
	var w = canvas.width();
	var h = canvas.height();
	var aspect = w / h;
	
	var proj = mat4.create();
	mat4.identity(proj);
	mat4.perspective(fov, aspect, 0.001, 1000.0, proj);
	
	var modelView = mat4.create();
	mat4.identity(modelView);
//	mat4.translate(modelView, [0, 0, -1.5], modelView);
	mat4.rotate(modelView, ry*Math.PI*2/360, [1, 0, 0], modelView);
	mat4.rotate(modelView, rx*Math.PI*2/360, [0, 1, 0], modelView);
	
	var mvpMatrix = mat4.create();
	mat4.multiply(modelView, proj, mvpMatrix);
	
	gl.uniformMatrix4fv(program.modelViewMatrix, false, modelView);
	gl.uniformMatrix4fv(program.projMatrix, false, proj);
	gl.uniformMatrix4fv(program.mvpMatrix, false, mvpMatrix);
	
	$.each([3, 2], function(i, stride){
		gl.enableVertexAttribArray(i);
		gl.bindBuffer(gl.ARRAY_BUFFER, vbuffers[i]);
		gl.vertexAttribPointer(i, stride, gl.FLOAT, false, 0, 0);
	});
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibuffer);
	
	gl.uniform1i(gl.getUniformLocation(program, "texture0"), 0);
	gl.enable(gl.TEXTURE_2D);
	gl.activeTexture(gl.TEXTURE0);
	gl.bindTexture(gl.TEXTURE_2D, texture);
	gl.drawElements(gl.TRIANGLES, numIndices, gl.UNSIGNED_SHORT, 0);
	
	gl.flush();
}
$(function(){
	var canvas = $("#main").get(0);
	$.each(["webgl", "experimental-webgl", "webkit-3d", "moz-webgl"], function(i, name){
		try {
			gl = canvas.getContext(name);
		} 
		catch (e) {
		}
		return !gl
	});
	if (!gl) {
		alert("WebGL がサポートされていません");
		return;
	}
	
	initialize();
	renderScene();
	
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
				fov -= d[0];
				if(fov < 1) fov = 1;
				if(fov > 170) fov = 170;
			}
		}
		renderScene();
		return false;
	};
	canvas.onmousedown = function(e){
		var event = windowEvent();
		button = event.button + 1;
		prev_position = getMousePosition();
		return false; // カーソル変更を防ぐ
	};
	document.onmouseup = function(e){
		button = false;
		return false; // コンテキストメニュー禁止
	}
	document.oncontextmenu = function(e){
		return false;
	}
	
	image.onload = function(){
		console.log("load texture", image.title, image.width, image.height);
		var canvas = $("#texture").get(0);
		var ctx = canvas.getContext("2d");
		ctx.fillStyle = "#00FF00";
		ctx.clearRect(0, 0, canvas.width, canvas.height);
		ctx.fillRect(0, 0, canvas.width, canvas.height);
		ctx.drawImage(image, 0, 0, canvas.width, canvas.height);
		
		// texture init
		gl.deleteTexture(texture);
		texture = gl.createTexture();
		gl.activeTexture(gl.TEXTURE0);
		gl.bindTexture(gl.TEXTURE_2D, texture);
		gl.pixelStorei(gl.UNPACK_ALIGNMENT, true);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
		
		gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, ctx.getImageData(0, 0, canvas.width, canvas.height));
		
		renderScene();
		
	}
	image.src = "./thumbnail.jpg";
	
	
	var req = new XMLHttpRequest();
	req.open('GET', "./sample.dds", true);
	req.overrideMimeType('text/plain; charset=x-user-defined')
	req.onload = function(){
		var dds = new DDS(gl, req.responseText);
		
		var canvas = $("#ddstest").get(0);
		canvas.width = dds.width;
		canvas.height = dds.height;
		var ctx = canvas.getContext("2d");
		var teximage = ctx.createImageData(dds.width, dds.height);		
//		console.log(dds.images, dds.images[0], dds.width*dds.height*4);
		for(var i = 0; i < dds.width*dds.height*4; i++)
			teximage.data[i] = dds.images[1][i];
		ctx.putImageData(teximage, 0, 0);	
		
		// texture init
		gl.deleteTexture(texture);
		texture = gl.createTexture();
		gl.activeTexture(gl.TEXTURE0);
		gl.bindTexture(gl.TEXTURE_2D, texture);
		gl.pixelStorei(gl.UNPACK_ALIGNMENT, true);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
		
		gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, ctx.getImageData(0, 0, canvas.width, canvas.height));
		
		renderScene();
		
	}
	req.send();
	
	var select = $("#select").get(0);
	console.log(select.options);
	for(var i = 100; i < 110; i++)
		select.options[select.length] = new Option(i, i);
	for(var i = 120; i < 130; i++)
		select.options[select.length] = new Option(i, i);
	select.size = 20;
	
	select.onclick = select.onchange = function(e){
		image.src = "./hdr/"+select.value+".jpg";
	}
	
});