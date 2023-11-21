	var vidR = document.getElementById("video-channel-1"),
	  vidG = document.getElementById("video-channel-2"),
	  vidB = document.getElementById("video-channel-3"),
	  button = document.getElementById('play-pause'),

	  totalTimeSpan = document.querySelector('.totalTime'),
	  current_R_TimeSpan = document.getElementById('current_R_Time'),
	  current_G_TimeSpan = document.getElementById('current_G_Time'),
	  current_B_TimeSpan = document.getElementById('current_B_Time'),
	  current_G_Deviation = document.getElementById('current_G_Deviation'),
	  current_B_Deviation = document.getElementById('current_B_Deviation'),
	  totalFramesSpan = document.querySelector('.totalFrames'),
	  current_R_FrameSpan = document.getElementById('current_R_Frame'),
	  current_G_FrameSpan = document.getElementById('current_G_Frame'),
	  current_B_FrameSpan = document.getElementById('current_B_Frame'),
	  fps = 25,
	  //currentFrame = 0 - 1 ,
	  duration, totalFrames, frameClock;

	function readyVidInterlace() {
	  // window.alert("Can start playing video");

	  if ((vidR.readyState >= 3) && (vidG.readyState >= 3) && (vidB.readyState >= 3)) {

	    duration = vidR.duration;
	    totalFrames = Math.round(duration * fps) - 1;
	    totalFramesSpan.innerHTML = totalFrames;
	    totalTimeSpan.innerHTML = duration;

	  }
	}

	function vidDeviationControl() {

	  function updateVideoStats() {
	    var current_R_Time, current_G_Time, current_B_Time,
	      current_R_Frame, current_G_Frame, current_B_Frame;

	    if ((!vidR.seeking) && (!vidG.seeking) && (!vidB.seeking)) {
	      current_R_Time = vidR.currentTime;
	      current_G_Time = vidG.currentTime;
	      current_B_Time = vidB.currentTime;
	      current_R_Frame = Math.round(vidR.currentTime * fps);
	      current_G_Frame = Math.round(vidG.currentTime * fps);
	      current_B_Frame = Math.round(vidB.currentTime * fps);

	      current_R_TimeSpan.innerHTML = current_R_Time;
	      current_G_TimeSpan.innerHTML = current_G_Time;
	      current_B_TimeSpan.innerHTML = current_B_Time;
	      current_R_FrameSpan.innerHTML = current_R_Frame;
	      current_G_FrameSpan.innerHTML = current_G_Frame;
	      current_B_FrameSpan.innerHTML = current_B_Frame;

	      current_G_Deviation.innerHTML = vidR.currentTime - vidG.currentTime;
	      current_B_Deviation.innerHTML = vidR.currentTime - vidB.currentTime;
	    } else {
	      return;
	    }
	  }

	  if ((Math.abs(vidR.currentTime - vidG.currentTime) > 0.05) || (Math.abs(vidR.currentTime - vidB.currentTime) > 0.05)) {
	    //document.getElementById('current_G_Deviation').css("background-color", "red");
	    //document.getElementById('current_B_Deviation').css("background-color", "red");
	    cancelAnimationFrame(vidDeviationControl);

	    vidR.pause();
	    vidG.pause();
	    vidB.pause();

	    // set all 3 before new AnimationFrame is drawn.
	    vidB.currentTime = vidR.currentTime;
	    vidG.currentTime = vidB.currentTime;
	    vidR.currentTime = vidG.currentTime;

	    vidB.play();
	    vidG.play();
	    vidR.play();

	    updateVideoStats();
	  } else {
	    updateVideoStats();
	  }

	  if (vidR.paused || vidR.ended) {
	    cancelAnimationFrame(vidDeviationControl);
	    document.getElementById('play-pause').innerHTML = 'Start';
	    return;
	  } else {
	    requestAnimationFrame(vidDeviationControl);
	  }
	}

	function startVideoInterlace() {
	  vidR.play();
	  vidG.play();
	  vidB.play();
	  vidDeviationControl();
	  //vidCanvasControl();
	}

	function stopVideoInterlace() {
	  vidR.pause();
	  vidG.pause();
	  vidB.pause();
	}

	document.getElementById('play-pause').onclick = function(e) {
	  e.preventDefault();
	  if (vidR.paused || vidR.ended) {
	    startVideoInterlace();
	    document.getElementById('play-pause').innerHTML = 'Pause';
	  } else {
	    stopVideoInterlace();
	    document.getElementById('play-pause').innerHTML = 'Play';
	  }
	};