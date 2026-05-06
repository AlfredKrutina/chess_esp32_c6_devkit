(function () {
  var ytFrame = document.querySelector(".hero__video-frame[data-youtube-id]");
  if (
    ytFrame &&
    !window.matchMedia("(prefers-reduced-motion: reduce)").matches
  ) {
    var p = window.location.protocol;
    if (p === "http:" || p === "https:") {
      var vid = ytFrame.getAttribute("data-youtube-id");
      if (vid && /^[\w-]{11}$/.test(vid)) {
        var q =
          "autoplay=1&mute=1&loop=1&playlist=" +
          encodeURIComponent(vid) +
          "&playsinline=1&rel=0";
        var src =
          "https://www.youtube-nocookie.com/embed/" +
          encodeURIComponent(vid) +
          "?" +
          q;
        ytFrame.innerHTML =
          '<iframe src="' +
          src +
          '" title="CzechMate — představující video na YouTube" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen loading="eager"></iframe>';
        var iframe = ytFrame.querySelector("iframe");
        if (iframe) {
          iframe.style.cssText =
            "position:absolute;inset:0;width:100%;height:100%;border:0;";
        }
      }
    }
  }

  var toggle = document.querySelector("[data-nav-toggle]");
  var mobile = document.querySelector("[data-nav-mobile]");
  if (toggle && mobile) {
    toggle.addEventListener("click", function () {
      var open = mobile.classList.toggle("is-open");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
    mobile.querySelectorAll("a").forEach(function (link) {
      link.addEventListener("click", function () {
        mobile.classList.remove("is-open");
        toggle.setAttribute("aria-expanded", "false");
      });
    });
  }

  if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
    document.querySelectorAll("video[data-hero-video]").forEach(function (v) {
      v.removeAttribute("autoplay");
      try {
        v.pause();
      } catch (e) {}
    });
  }
})();
