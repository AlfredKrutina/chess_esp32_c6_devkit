(function () {
  /* Po reloadu vždy nahoru (kromě odkazu s #kotvou); bez posunu při načítání médií. */
  (function initScrollTopOnLoad() {
    function scrollTopUnlessHash() {
      if (location.hash) return;
      window.scrollTo(0, 0);
    }

    if ("scrollRestoration" in history) {
      history.scrollRestoration = "manual";
    }

    scrollTopUnlessHash();
    document.addEventListener("DOMContentLoaded", scrollTopUnlessHash);
    window.addEventListener("load", scrollTopUnlessHash);
    window.addEventListener("pageshow", function (ev) {
      if (!location.hash && !ev.persisted) {
        scrollTopUnlessHash();
      }
    });
  })();

  /** Fade + lehký posun dialogu a backdropu; Escape přes `cancel` + preventDefault */
  function bindAnimatedModal(dialog) {
    var reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    var maxWaitMs = 480;
    if (!dialog) {
      return { open: function () {}, close: function () {} };
    }
    if (reduce || typeof dialog.showModal !== "function") {
      return {
        open: function () {
          if (typeof dialog.showModal === "function") dialog.showModal();
        },
        close: function () {
          if (dialog.open && typeof dialog.close === "function") dialog.close();
        },
      };
    }

    function openModal() {
      dialog.classList.remove("cz-modal--visible");
      dialog.showModal();
      requestAnimationFrame(function () {
        requestAnimationFrame(function () {
          dialog.classList.add("cz-modal--visible");
        });
      });
    }

    function closeModal() {
      if (!dialog.open) return;
      dialog.classList.remove("cz-modal--visible");
      var done = false;
      function finish() {
        if (done) return;
        done = true;
        dialog.removeEventListener("transitionend", onEnd);
        try {
          clearTimeout(tid);
        } catch (e) {}
        if (dialog.open && typeof dialog.close === "function") dialog.close();
      }
      function onEnd(ev) {
        if (ev.target !== dialog) return;
        if (ev.propertyName !== "opacity" && ev.propertyName !== "transform") {
          return;
        }
        finish();
      }
      dialog.addEventListener("transitionend", onEnd);
      var tid = setTimeout(finish, maxWaitMs);
    }

    dialog.addEventListener("close", function () {
      dialog.classList.remove("cz-modal--visible");
    });

    dialog.addEventListener("cancel", function (ev) {
      ev.preventDefault();
      closeModal();
    });

    return { open: openModal, close: closeModal };
  }

  /** Spinner u videí: is-loading → is-ready po canplay / náhledu. */
  var czVideoMedia = (function () {
    function rootFor(el) {
      if (!el) return null;
      return el.closest("[data-video-media]");
    }

    function markLoading(media) {
      if (!media) return;
      media.classList.add("is-loading");
      media.classList.remove("is-ready", "is-error");
      media.setAttribute("aria-busy", "true");
    }

    function markReady(media) {
      if (!media || media.classList.contains("is-ready")) return;
      media.classList.remove("is-loading", "is-error");
      media.classList.add("is-ready");
      media.removeAttribute("aria-busy");
    }

    function markError(media) {
      if (!media) return;
      media.classList.remove("is-loading");
      media.classList.add("is-error", "is-ready");
      media.removeAttribute("aria-busy");
    }

    function bindVideo(v) {
      var media = rootFor(v);
      if (!media || v.dataset.videoMediaBound === "1") return;
      v.dataset.videoMediaBound = "1";
      v.removeAttribute("poster");
      if (!v.classList.contains("split__video--controls-on-click")) {
        v.controls = false;
      }
      markLoading(media);

      function readyThreshold() {
        return 2;
      }

      function tryMarkReady() {
        if (v.readyState >= readyThreshold()) markReady(media);
      }

      v.addEventListener("loadedmetadata", tryMarkReady);
      v.addEventListener("loadeddata", tryMarkReady);
      v.addEventListener("canplay", tryMarkReady);
      v.addEventListener("playing", tryMarkReady, { once: true });
      v.addEventListener("error", function () {
        markError(media);
      });
      tryMarkReady();
    }

    function init() {
      document.querySelectorAll("[data-video-media]").forEach(function (media) {
        if (!media.classList.contains("is-ready")) markLoading(media);
      });
      document.querySelectorAll("[data-video-media] video").forEach(bindVideo);
    }

    return { markLoading: markLoading, markReady: markReady, markError: markError, bindVideo: bindVideo, init: init };
  })();

  czVideoMedia.init();

  /* Welcome MP4: stahovat hned po startu skriptu (priorita před YouTube). */
  (function initWelcomeVideoEarlyLoad() {
    var v = document.getElementById("czm-v2-welcome-video");
    if (!v || window.matchMedia("(prefers-reduced-motion: reduce)").matches) return;
    v.preload = "auto";
    if (v.networkState === HTMLMediaElement.NETWORK_EMPTY) {
      try {
        v.load();
      } catch (eW) {}
    }
  })();

  /* Hero YouTube: autoplay bez ovládací lišty; controls=1 až po kliknutí na video. */
  (function initHeroYoutubeAutoplay() {
    var ytFrame = document.querySelector(".hero__video-frame[data-youtube-id]");
    if (!ytFrame || window.location.protocol === "file:") return;
    var iframe = ytFrame.querySelector(".hero__video-iframe");
    if (!iframe) return;
    czVideoMedia.markLoading(ytFrame);
    var vid = ytFrame.getAttribute("data-youtube-id");
    if (!vid || !/^[\w-]{11}$/.test(vid)) return;

    var autoplay = !window.matchMedia("(prefers-reduced-motion: reduce)").matches;

    function buildEmbedUrl(withControls) {
      var params = [
        "controls=" + (withControls ? "1" : "0"),
        "fs=1",
        "playsinline=1",
        "rel=0",
        "modestbranding=1",
        "iv_load_policy=3",
      ];
      if (autoplay) {
        params.unshift("autoplay=1", "mute=1");
      }
      try {
        if (window.location.origin && window.location.origin !== "null") {
          params.push("origin=" + encodeURIComponent(window.location.origin));
        }
      } catch (eO) {}
      return (
        "https://www.youtube.com/embed/" + encodeURIComponent(vid) + "?" + params.join("&")
      );
    }

    function mountYoutubeEmbed() {
      iframe.src = buildEmbedUrl(false);
      iframe.setAttribute(
        "allow",
        (autoplay ? "autoplay; " : "") +
          "accelerometer; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share; fullscreen"
      );
    }
    iframe.addEventListener("load", function () {
      czVideoMedia.markReady(ytFrame);
    });
    window.setTimeout(function () {
      czVideoMedia.markReady(ytFrame);
    }, 10000);
    /* Krátké zpoždění YouTube — priorita stahu welcome MP4 (~4 MB) hned pod hero. */
    window.setTimeout(mountYoutubeEmbed, 180);

    var shield = ytFrame.querySelector("[data-yt-click-shield]");
    if (!shield) return;

    function activateYoutubeControls() {
      if (ytFrame.classList.contains("is-yt-interactive")) return;
      ytFrame.classList.add("is-yt-interactive");
      iframe.src = buildEmbedUrl(true);
      iframe.setAttribute("tabindex", "0");
      try {
        iframe.focus();
      } catch (eF) {}
    }

    shield.addEventListener("click", activateYoutubeControls);
    shield.addEventListener("keydown", function (ev) {
      if (ev.key === "Enter" || ev.key === " ") {
        ev.preventDefault();
        activateYoutubeControls();
      }
    });
  })();

  /* file://: YouTube embed často selže; nahrazeno statickým obrázkem z CDN. */
  (function initHeroYoutubeFileFallback() {
    var ytFrame = document.querySelector(".hero__video-frame[data-youtube-id]");
    if (!ytFrame || window.location.protocol !== "file:") return;
    var vid = ytFrame.getAttribute("data-youtube-id");
    if (!vid || !/^[\w-]{11}$/.test(vid)) return;
    ytFrame.innerHTML =
      '<div class="hero__video-fallback">' +
      '<img class="hero__video-fallback__img" src="https://img.youtube.com/vi/' +
      encodeURIComponent(vid) +
      '/maxresdefault.jpg" alt="CzechMate — představení produktu" width="1280" height="720" loading="lazy" decoding="async" onerror="this.onerror=null;this.src=\'https://img.youtube.com/vi/' +
      encodeURIComponent(vid) +
      "/hqdefault.jpg'\">" +
      "</div>";
  })();

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

  /* Přímé stažení APK/DMG z nejnovějšího GitHub release (API → browser_download_url) */
  (function initReleaseDownloadLinks() {
    var api =
      "https://api.github.com/repos/alfredkrutina/chess_esp32_c6_devkit/releases/latest";
    var apkAnchors = document.querySelectorAll("[data-dl-apk]");
    var dmgAnchors = document.querySelectorAll("[data-dl-dmg]");
    var winAnchors = document.querySelectorAll("[data-dl-windows]");
    if (!apkAnchors.length && !dmgAnchors.length && !winAnchors.length) return;
    if (!window.fetch) return;

    fetch(api, { headers: { Accept: "application/vnd.github+json" } })
      .then(function (res) {
        if (!res.ok) throw new Error("releases " + res.status);
        return res.json();
      })
      .then(function (data) {
        var assets = (data && data.assets) || [];
        var apk = assets.find(function (a) {
          return /\.apk$/i.test(a.name);
        });
        var dmg = assets.find(function (a) {
          return /\.dmg$/i.test(a.name);
        });
        var win = assets.find(function (a) {
          return /windows-setup\.exe$/i.test(a.name);
        });
        apkAnchors.forEach(function (el) {
          if (apk && apk.browser_download_url) {
            el.href = apk.browser_download_url;
            if (apk.name) el.setAttribute("download", apk.name);
          }
        });
        dmgAnchors.forEach(function (el) {
          if (dmg && dmg.browser_download_url) {
            el.href = dmg.browser_download_url;
            if (dmg.name) el.setAttribute("download", dmg.name);
          }
        });
        winAnchors.forEach(function (el) {
          if (win && win.browser_download_url) {
            el.href = win.browser_download_url;
            if (win.name) el.setAttribute("download", win.name);
          }
        });
      })
      .catch(function () {
        /* Fallback zůstane href z HTML (stránka release) */
      });
  })();

  if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
    document.querySelectorAll("video[data-lazy-local]").forEach(function (v) {
      v.removeAttribute("autoplay");
      try {
        v.pause();
      } catch (e) {}
    });
  }

  /* #czm-v2-welcome-video: scroll → přehrání; hover nic; klik → znovu od začátku; bez ovládací lišty. */
  (function initWelcomeV2PlayWhenVisible() {
    var v = document.getElementById("czm-v2-welcome-video");
    var media = v && v.closest(".welcome-v2__media");
    if (!v || !media || !v.hasAttribute("data-play-when-visible")) return;
    v.removeAttribute("loop");
    v.controls = false;

    function blockNativeVideoUi(ev) {
      ev.preventDefault();
    }
    v.addEventListener("click", blockNativeVideoUi);
    v.addEventListener("dblclick", blockNativeVideoUi);
    v.addEventListener("contextmenu", blockNativeVideoUi);

    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      try {
        v.pause();
      } catch (e0) {}
      if (v.hasAttribute("data-welcome-replay-on-click")) {
        media.addEventListener("click", function (ev) {
          ev.preventDefault();
          try {
            v.currentTime = 0;
          } catch (eR) {}
          var pr = v.play();
          if (pr && typeof pr.catch === "function") {
            pr.catch(function () {});
          }
        });
      }
      return;
    }
    if (!("IntersectionObserver" in window)) return;

    var visibleRatio = 0.92;
    var hideRatio = 0.72;
    var ratioRaw = parseFloat(v.getAttribute("data-play-visible-ratio"));
    if (isFinite(ratioRaw) && ratioRaw > 0 && ratioRaw <= 1) {
      visibleRatio = ratioRaw;
      hideRatio = Math.max(0.5, visibleRatio - 0.18);
    }

    var mediaReady = false;
    var inView = false;

    function ensureLoadedOnce() {
      if (mediaReady) return;
      mediaReady = true;
      v.preload = "auto";
    }

    function tryPlay() {
      ensureLoadedOnce();
      var pr = v.play();
      if (pr && typeof pr.catch === "function") {
        pr.catch(function () {});
      }
    }

    function tryPause() {
      try {
        v.pause();
      } catch (e1) {}
    }

    v.addEventListener("ended", function () {
      try {
        v.pause();
      } catch (e2) {}
    });

    var io = new IntersectionObserver(
      function (entries) {
        entries.forEach(function (en) {
          var ratio = en.intersectionRatio;
          if (!inView && ratio >= visibleRatio) {
            inView = true;
            if (!v.ended) {
              tryPlay();
            }
          } else if (inView && ratio < hideRatio) {
            inView = false;
            tryPause();
          }
        });
      },
      {
        root: null,
        threshold: [0, 0.25, 0.5, hideRatio, visibleRatio, 1],
        rootMargin: "0px",
      }
    );
    io.observe(v);

    if (v.hasAttribute("data-welcome-replay-on-click")) {
      media.addEventListener("click", function (ev) {
        ev.preventDefault();
        if (!v.paused && !v.ended) return;
        ensureLoadedOnce();
        try {
          v.currentTime = 0;
        } catch (e3) {}
        tryPlay();
      });
    }
  })();

  /* MP4: lazy + rozestup startu stahů (dva soubory nezačnou naráz). */
  (function initLazyLocalVideos() {
    var vids = document.querySelectorAll("video[data-lazy-local]");
    if (!vids.length) return;
    var reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    var saveData = false;
    try {
      var conn = navigator.connection || navigator.mozConnection || navigator.webkitConnection;
      saveData = !!(conn && conn.saveData);
    } catch (eC) {}

    var rootMargin = saveData ? "120px 0px" : "720px 0px";
    var hydrateGapMs = saveData ? 180 : 80;
    var nextVideoLoadAt = 0;

    function nearViewport(el) {
      var r = el.getBoundingClientRect();
      var m = saveData ? 120 : 480;
      var vh = window.innerHeight || document.documentElement.clientHeight;
      var vw = window.innerWidth || document.documentElement.clientWidth;
      return (
        r.top < vh + m &&
        r.bottom > -m &&
        r.left < vw + 100 &&
        r.right > -100
      );
    }

    function hydrate(v) {
      if (v.dataset.lazyLocalHydrated === "1") return;
      var media = v.closest("[data-video-media]");
      if (media) czVideoMedia.markLoading(media);
      v.preload = "auto";
      v.dataset.lazyLocalHydrated = "1";
      v.querySelectorAll("source[data-src]").forEach(function (s) {
        var url = s.getAttribute("data-src");
        if (!url) return;
        s.src = url;
        s.removeAttribute("data-src");
      });
      czVideoMedia.bindVideo(v);
      try {
        v.load();
      } catch (eL) {}
      if (!reduce && v.getAttribute("autoplay") !== null) {
        var pr = v.play();
        if (pr && typeof pr.catch === "function") {
          pr.catch(function () {});
        }
      }
    }

    function scheduleHydrate(v) {
      if (v.dataset.lazyLocalHydrated === "1") return;
      var now = Date.now();
      var delay = Math.max(0, nextVideoLoadAt - now);
      nextVideoLoadAt = Math.max(now, nextVideoLoadAt) + hydrateGapMs;
      window.setTimeout(function () {
        hydrate(v);
      }, delay);
    }

    var pending = [];
    vids.forEach(function (v) {
      if (nearViewport(v)) {
        scheduleHydrate(v);
      } else {
        pending.push(v);
      }
    });

    /* Náhled #ai-kouc-app-preview je nad #app-phone-demo-video — MP4 načíst dřív než sjezd k přehrávači. */
    function bindAiKoucEarlyAppVideoLoad() {
      var appDemo = document.getElementById("app-phone-demo-video");
      var aiKoucSection = document.getElementById("ai-kouc");
      if (!appDemo || !aiKoucSection || !appDemo.hasAttribute("data-lazy-local")) return;

      function preloadAppDemoForPreview() {
        scheduleHydrate(appDemo);
      }

      var marginPx = saveData ? 280 : 1600;

      function aiSectionNear() {
        var r = aiKoucSection.getBoundingClientRect();
        var vh = window.innerHeight || document.documentElement.clientHeight;
        return r.top < vh + marginPx && r.bottom > -160;
      }

      if (aiSectionNear()) {
        preloadAppDemoForPreview();
      }

      if (!("IntersectionObserver" in window)) return;

      var aiIo = new IntersectionObserver(
        function (entries) {
          entries.forEach(function (en) {
            if (en.isIntersecting) {
              preloadAppDemoForPreview();
              aiIo.disconnect();
            }
          });
        },
        { rootMargin: marginPx + "px 0px 480px 0px", threshold: 0 }
      );
      aiIo.observe(aiKoucSection);
    }

    bindAiKoucEarlyAppVideoLoad();

    if (!pending.length) return;
    if (!("IntersectionObserver" in window)) {
      pending.forEach(function (v, i) {
        window.setTimeout(function () {
          scheduleHydrate(v);
        }, i * hydrateGapMs);
      });
      return;
    }

    var io = new IntersectionObserver(
      function (entries) {
        entries.forEach(function (en) {
          if (en.isIntersecting) {
            scheduleHydrate(en.target);
            io.unobserve(en.target);
          }
        });
      },
      { rootMargin: rootMargin, threshold: 0.01 }
    );
    pending.forEach(function (v) {
      io.observe(v);
    });
  })();

  /* #ai-kouc-app-preview: snímek z MP4 v čase data-ai-preview-at (0–1, výchozí 0.52 — konec bývá černý/fade). */
  (function initAiKoucVideoPreviewFrame() {
    var demo = document.getElementById("app-phone-demo-video");
    var preview = document.getElementById("ai-kouc-app-preview");
    var aiKoucSection = document.getElementById("ai-kouc");
    var reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

    if (!demo || !preview) return;

    var done = false;
    var scheduleTimer = null;

    function ensureDemoSourceForCapture() {
      if (demo.dataset.lazyLocalHydrated === "1") return;
      demo.querySelectorAll("source[data-src]").forEach(function (s) {
        var url = s.getAttribute("data-src");
        if (!url) return;
        s.src = url;
        s.removeAttribute("data-src");
      });
      demo.dataset.lazyLocalHydrated = "1";
      try {
        demo.load();
      } catch (eH) {}
    }

    /* Záloha: pokud lazy-loader ještě neběžel, spustit zdroj při blížící se sekci AI kouč. */
    if (aiKoucSection && "IntersectionObserver" in window) {
      var capturePreloadIo = new IntersectionObserver(
        function (entries) {
          entries.forEach(function (en) {
            if (en.isIntersecting) {
              ensureDemoSourceForCapture();
              capturePreloadIo.disconnect();
            }
          });
        },
        { rootMargin: "1600px 0px 480px 0px", threshold: 0 }
      );
      capturePreloadIo.observe(aiKoucSection);
    } else if (aiKoucSection) {
      var r0 = aiKoucSection.getBoundingClientRect();
      var vh0 = window.innerHeight || document.documentElement.clientHeight;
      if (r0.top < vh0 + 1600) ensureDemoSourceForCapture();
    }

    function scheduleCapture() {
      if (done || scheduleTimer) return;
      if (demo.readyState < 1 || !demo.videoWidth) {
        if (demo.readyState < 1) ensureDemoSourceForCapture();
        return;
      }
      scheduleTimer = window.setTimeout(function () {
        scheduleTimer = null;
        capturePreviewFrame();
      }, reduce ? 900 : 400);
    }

    function capturePreviewFrame() {
      if (done) return;
      var w = demo.videoWidth;
      var h = demo.videoHeight;
      var d = demo.duration;
      if (!w || !h || !isFinite(d) || d <= 0) return;

      var wasPlaying = !demo.paused;
      var savedTime = demo.currentTime;
      var fracRaw = parseFloat(demo.getAttribute("data-ai-preview-at"));
      var frac =
        isFinite(fracRaw) && fracRaw >= 0.08 && fracRaw <= 0.92 ? fracRaw : 0.52;
      var target = frac * d;
      if (target < 0.12) target = 0.12;
      if (target > d - 0.08) target = d - 0.08;
      var applied = false;

      function resume() {
        try {
          demo.currentTime = wasPlaying ? 0 : savedTime;
          if (wasPlaying) {
            demo.play();
          }
        } catch (eR) {}
      }

      function applyFrame() {
        if (applied) return;
        applied = true;
        demo.removeEventListener("seeked", onSeeked);
        try {
          var canv = document.createElement("canvas");
          var maxPreviewW = 800;
          var sc = w > maxPreviewW ? maxPreviewW / w : 1;
          var outW = Math.max(1, Math.round(w * sc));
          var outH = Math.max(1, Math.round(h * sc));
          canv.width = outW;
          canv.height = outH;
          var ctx = canv.getContext("2d");
          if (ctx) {
            ctx.drawImage(demo, 0, 0, w, h, 0, 0, outW, outH);
            preview.src = canv.toDataURL("image/jpeg", 0.82);
            preview.alt = "CzechMate — AI kouč v aplikaci";
            done = true;
            var previewMedia = preview.closest("[data-video-media]");
            if (previewMedia) czVideoMedia.markReady(previewMedia);
          }
        } catch (e1) {
          /* např. bezpečnostní kontext / poškozený zdroj */
        }
        resume();
      }

      function onSeeked() {
        applyFrame();
      }

      demo.addEventListener("seeked", onSeeked);
      try {
        demo.pause();
        demo.currentTime = target;
      } catch (e2) {
        demo.removeEventListener("seeked", onSeeked);
        resume();
        return;
      }

      window.setTimeout(function () {
        if (!done) {
          applyFrame();
        }
      }, 320);
    }

    demo.addEventListener("loadedmetadata", scheduleCapture);
    demo.addEventListener("loadeddata", scheduleCapture);
    demo.addEventListener("canplay", scheduleCapture);
    demo.addEventListener("playing", scheduleCapture, { once: true });
    if (demo.readyState >= 1 && demo.videoWidth) {
      scheduleCapture();
    }
  })();

  /* Ukázka aplikace (#app-phone-demo-video): přehrávání když je video dostatečně vidět ve viewportu. */
  (function initAppDemoVideoPlayWhenVisible() {
    var v = document.getElementById("app-phone-demo-video");
    if (
      !v ||
      (!v.hasAttribute("data-play-when-visible") && !v.hasAttribute("data-play-on-hover"))
    )
      return;
    v.removeAttribute("autoplay");
    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      try {
        v.pause();
      } catch (e0) {}
      return;
    }
    if (!("IntersectionObserver" in window)) return;

    function ensureMediaSource(vid) {
      if (vid.dataset.lazyLocalHydrated === "1") return;
      vid.querySelectorAll("source[data-src]").forEach(function (s) {
        var url = s.getAttribute("data-src");
        if (!url) return;
        s.src = url;
        s.removeAttribute("data-src");
      });
      vid.dataset.lazyLocalHydrated = "1";
      try {
        vid.load();
      } catch (eL) {}
    }

    function tryPlay() {
      ensureMediaSource(v);
      var pr = v.play();
      if (pr && typeof pr.catch === "function") {
        pr.catch(function () {});
      }
    }

    function tryPause() {
      try {
        v.pause();
      } catch (e1) {}
    }

    var io = new IntersectionObserver(
      function (entries) {
        entries.forEach(function (en) {
          if (en.isIntersecting) tryPlay();
          else tryPause();
        });
      },
      {
        root: null,
        threshold: 0.28,
        rootMargin: "0px 0px -6% 0px",
      }
    );
    io.observe(v);
  })();

  /* LED loop (#deska-uci): nepauzovat — žádné ovládání, po pause okamžitě znovu přehrát. */
  (function initLedLoopNoPause() {
    var v = document.getElementById("czm-v2-led-loop-video");
    if (!v || !v.hasAttribute("data-no-pause")) return;
    v.controls = false;

    function blockNativeVideoUi(ev) {
      ev.preventDefault();
    }
    v.addEventListener("click", blockNativeVideoUi);
    v.addEventListener("dblclick", blockNativeVideoUi);
    v.addEventListener("contextmenu", blockNativeVideoUi);

    var reduceMq = window.matchMedia("(prefers-reduced-motion: reduce)");

    function resumeIfAllowed() {
      if (reduceMq.matches) return;
      if (v.ended || !v.paused) return;
      var pr = v.play();
      if (pr && typeof pr.catch === "function") {
        pr.catch(function () {});
      }
    }

    v.addEventListener("pause", resumeIfAllowed);
    document.addEventListener("visibilitychange", function () {
      if (!document.hidden) resumeIfAllowed();
    });
  })();

  /* Ovládání videa: nativní panel až po kliknutí na samotné video */
  (function initVideoControlsOnClick() {
    document.querySelectorAll("video.split__video--controls-on-click").forEach(function (v) {
      v.controls = false;
      function revealControls() {
        if (v.controls) return;
        v.controls = true;
        v.removeEventListener("click", revealControls);
        if (v.getAttribute("tabindex") === "-1") {
          v.setAttribute("tabindex", "0");
        }
        try {
          v.focus();
        } catch (eF) {}
      }
      v.addEventListener("click", revealControls);
    });
  })();

  /* Modal předobjednávky (<dialog>) */
  var preorderDlg = document.getElementById("preorder-dialog");
  var preorderAnim = bindAnimatedModal(
    preorderDlg && typeof preorderDlg.showModal === "function"
      ? preorderDlg
      : null
  );
  var openPreorder = document.querySelector("[data-open-preorder]");
  var closePreorder = document.querySelector("[data-close-preorder]");
  if (preorderDlg && typeof preorderDlg.showModal === "function") {
    if (openPreorder) {
      openPreorder.addEventListener("click", function () {
        preorderAnim.open();
      });
    }
    if (closePreorder) {
      closePreorder.addEventListener("click", function () {
        preorderAnim.close();
      });
    }
    preorderDlg.addEventListener("click", function (ev) {
      if (ev.target === preorderDlg) {
        preorderAnim.close();
      }
    });
  } else if (openPreorder && preorderDlg) {
    /* Fallback bez nativního <dialog> */
    openPreorder.addEventListener("click", function () {
      preorderDlg.setAttribute("open", "");
      preorderDlg.style.display = "block";
    });
    if (closePreorder) {
      closePreorder.addEventListener("click", function () {
        preorderDlg.removeAttribute("open");
        preorderDlg.style.display = "none";
      });
    }
  }

  /* Detail funkcí — modální okno (karty v sekci Aplikace) */
  (function initFeatureDetailModal() {
    function feIcon(pathsD) {
      return (
        '<span class="feature-detail-icon" aria-hidden="true"><svg width="26" height="26" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg" stroke="currentColor" stroke-width="1.65" stroke-linecap="round" stroke-linejoin="round">' +
        pathsD +
        "</svg></span>"
      );
    }

    var ic = {
      sun:
        '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41"/>',
      layers:
        '<path d="M12 2L2 7l10 5 10-5-10-5z"/><path d="M2 17l10 5 10-5"/><path d="M2 12l10 5 10-5"/>',
      book: '<path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/>',
      gauge:
        '<path d="M12 20a8 8 0 1 0-8-8 8 8 0 0 0 8 8z"/><path d="M12 12l4-2"/><path d="M12 12V6"/>',
      list: '<path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/>',
      chat:
        '<path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>',
      cloud:
        '<path d="M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z"/>',
      shield:
        '<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/><path d="M9 12l2 2 4-4"/>',
      bluetooth:
        '<path d="M6.5 6.5l11 11M17.5 6.5l-11 11M12 3v7l4-3-4-3zM12 21v-7l-4 3 4 3z"/>',
      wifi: '<path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><path d="M12 20h.01"/>',
      sync:
        '<path d="M21 2v6h-6"/><path d="M3 12a9 9 0 0 1 15-6.7L21 8"/><path d="M3 22v-6h6"/><path d="M21 12a9 9 0 0 1-15 6.7L3 16"/>',
      globe:
        '<circle cx="12" cy="12" r="10"/><path d="M2 12h20"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/>',
      download:
        '<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/>',
      led: '<rect x="2" y="4" width="20" height="16" rx="2"/><path d="M7 8h2M11 8h2M15 8h2M7 12h4M13 12h2"/>',
      branch:
        '<line x1="6" y1="3" x2="6" y2="15"/><circle cx="18" cy="6" r="3"/><circle cx="6" cy="18" r="3"/><path d="M18 9a9 9 0 0 1-9 9"/>',
      chip: '<rect x="4" y="4" width="16" height="16" rx="2"/><rect x="9" y="9" width="6" height="6"/><path d="M9 1v3M15 1v3M9 20v3M15 20v3M20 9h3M20 14h3M1 9h3M1 14h3"/>',
    };

    var FEATURE_PAGES = {
      hints: {
        title: "Nápovědy a výukové režimy",
        intro:
          "Světla na polích a aplikace říkají totéž: nemusíš překládat pozici z hlavy.",
        blocks: [
          {
            icon: feIcon(ic.sun),
            heading: "Co ti deska ukáže na místě",
            text:
              "Hall senzory poznají typ figurky na poli. LED ti pomůže u doporučeného tahu, šachu i matu, u promocí a při návratu pozornosti ke správné figurce. V aplikaci máš stejnou pozici s textem a analýzou.",
          },
          {
            icon: feIcon(ic.layers),
            heading: "Jedna partie, dvě obrazovky",
            text:
              "Historie tahů a výukové režimy v aplikaci kopírují stav z desky. Žádný „diagram vedle reality“, který by neodpovídal figurkám.",
          },
          {
            icon: feIcon(ic.book),
            heading: "Pro začátečníky i pokročilé",
            text:
              "Začátečník vidí jasné signály na desce. Pokročilejší si v aplikaci může stáhnout hlubší rozbor nebo kouče. Jedna soustava roste s úrovní bez přehazování nástrojů.",
          },
        ],
      },
      analysis: {
        title: "Analýza a Stockfish",
        intro:
          "Motor Stockfish doplní partii hodnocením a návrhy tahů; sílu si nastavíš podle úrovně.",
        blocks: [
          {
            icon: feIcon(ic.gauge),
            heading: "Silný motor, klidná výuka",
            text:
              "Stockfish má výkon nad lidský vrchol. V CzechMate z něj bereš hodnocení pozice a návrhy tahů, ale můžeš zvolit slabší linii, aby šlo o učení, ne o sérii jednostranných výsledků.",
          },
          {
            icon: feIcon(ic.list),
            heading: "Rozbor po partii i příprava",
            text:
              "V aplikaci máš varianty, řádky analýzy a srovnání s vlastním tahem. Hodí se na přípravu i na rozbor po tréninku.",
          },
          {
            icon: feIcon(ic.cloud),
            heading: "Stejná logika i z prohlížeče",
            text:
              "S Wi‑Fi a webem na desce můžeš část výuky nebo hru proti motoru spustit z prohlížeče. Partie běží stejně, mění se jen klient.",
          },
        ],
      },
      stockfish_a: {
        title: "Partie silnější než člověk",
        intro:
          "Soupeř silnější než kterýkoli živý hráč, ale nastavitelný tak, aby z partie něco zůstalo.",
        blocks: [
          {
            icon: feIcon(ic.gauge),
            heading: "Síla motoru pod kontrolou",
            text:
              "Stockfish v plné síle překoná každého živého hráče. Pro výuku ale zvolíš slabší linii a měkčí tempo, aby šlo číst hru a ne jen prohrávat.",
          },
          {
            icon: feIcon(ic.list),
            heading: "Co z toho máš při učení",
            text:
              "U tahů vidíš rozdíl mezi svojí volbou a návrhem motoru. Pro nácvik je často důležitější ten kontext než výsledek partie.",
          },
        ],
      },
      stockfish_b: {
        title: "Orientační hodnocení",
        intro:
          "Skóre z výpočtu pomůže číst nerovnováhu, nejlépe vedle variant v analýze.",
        blocks: [
          {
            icon: feIcon(ic.gauge),
            heading: "Čísla jako pomůcka, ne jako ortel",
            text:
              "Orientační skóre ukáže směr výhody na desce. Nejvíc platí ve střední hře a koncovkách, kde drobnosti vyrostou až po několika tazích.",
          },
          {
            icon: feIcon(ic.list),
            heading: "Vždy s kontextem tahů",
            text:
              "Hodnocení dává smysl vedle variant a řádků analýzy. Samotné číslo bez plánu na několik tahů dopředu málokdy stačí.",
          },
        ],
      },
      stockfish_c: {
        title: "Propojení deska ↔ aplikace",
        intro:
          "Firmware drží pravdu o pozici; deska, web i aplikace jen čtou stejný stav.",
        blocks: [
          {
            icon: feIcon(ic.layers),
            heading: "Jedna pozice všude",
            text:
              "LED, webové rozhraní i mobil ukazují totéž. Hall senzory ví, jaká figurka stojí kde, takže výuka na světlech a text na displeji sedí na skutečnou šachovnici.",
          },
          {
            icon: feIcon(ic.sync),
            heading: "Bluetooth nebo síť",
            text:
              "Nablízku často BLE. Doma nebo ve škole i Wi‑Fi a spojení přes HTTP nebo WebSocket, kde to firmware nabízí. Měníš klienta, ne pravidla partie.",
          },
        ],
      },
      coach: {
        title: "AI kouč / chat",
        intro:
          "Dotazy u aktuální pozice: plán, chyby i strategie. Jak hluboko jde výklad, závisí na nastavení.",
        blocks: [
          {
            icon: feIcon(ic.chat),
            heading: "Konverzace u partie",
            text:
              "Kouč reaguje na plány, chyby i obecné dotazy. Nastavíš si styl výkladu od školního klidu po klubovou hloubku.",
          },
          {
            icon: feIcon(ic.cloud),
            heading: "Asistent podle prostředí",
            text:
              "V nastavení lze napojit cloud nebo vlastní řešení podle pravidel školy nebo domova. Bez sítě zůstane základní offline nápověda z vestavěných šachových rad.",
          },
          {
            icon: feIcon(ic.shield),
            heading: "Škola a soukromí",
            text:
              "U cloudových služeb platí jejich podmínky a bezpečnost. Ve škole má smysl mít dopředu vyřešeného poskytovatele a účty podle vašich pravidel.",
          },
        ],
      },
      connect: {
        title: "Připojení k desce",
        intro:
          "V jedné místnosti Bluetooth; doma nebo ve třídě často Wi‑Fi a známá IP v síti.",
        blocks: [
          {
            icon: feIcon(ic.bluetooth),
            heading: "Bluetooth na dosah",
            text:
              "BLE stačí typicky v jedné místnosti a bez řešení IP. Partie a příkazy jedou jako u jiných zařízení v dosahu.",
          },
          {
            icon: feIcon(ic.wifi),
            heading: "Wi‑Fi v LAN",
            text:
              "V režimu klienta v síti aplikace komunikuje s deskou přes HTTP nebo WebSocket, kde to firmware nabízí. Běžné scénáře: domácnost, klubovna, učebna.",
          },
          {
            icon: feIcon(ic.sync),
            heading: "Stav vždy ze šachovnice",
            text:
              "Pravda o pozici žije ve firmwaru. LED a aplikace jen zobrazují totéž; krátké odchylky mohou nastat jen při animacích na desce.",
          },
        ],
      },
      webui: {
        title: "Web na desce",
        intro:
          "Zadej IP desky v prohlížeči a dostaneš rozhraní bez instalace dalšího programu.",
        blocks: [
          {
            icon: feIcon(ic.globe),
            heading: "Rozhraní z prohlížeče",
            text:
              "Na desce běží HTTP server. Výuku, nastavení i hru otevřeš z adresy šachovnice v prohlížeči — vhodné třeba v učebně na zařízení bez instalace aplikace.",
          },
          {
            icon: feIcon(ic.wifi),
            heading: "Stejná výuka přes kabel i bez",
            text:
              "Při stabilní síti jde motor i výuka z prohlížeče podobně jako z mobilní aplikace.",
          },
          {
            icon: feIcon(ic.layers),
            heading: "Web i mobil na stejném API",
            text:
              "Mobil, web i vlastní nástroje mohou použít stejné REST API. WebSocket je tam, kde ho firmware vystaví.",
          },
        ],
      },
      ota: {
        title: "Flash přímo z aplikace",
        intro:
          "Firmware desky nahraješ z aplikace bez programátoru a bez kabelů k čipu.",
        blocks: [
          {
            icon: feIcon(ic.download),
            heading: "Aktualizace bez laboratoře",
            text:
              "Nový firmware z aplikace po Wi‑Fi nebo v některých postupech po částech přes BLE. Na běžný update nepotřebuješ UART ani externí flasher.",
          },
          {
            icon: feIcon(ic.led),
            heading: "Vidíš průběh na LED",
            text:
              "OTA jde sledovat přímo na šachovnici. Po dokončení deska naběhne do nové verze ve flash.",
          },
          {
            icon: feIcon(ic.chip),
            heading: "Více způsobů dodání souboru",
            text:
              "Podle dostupnosti desky jde firmware stáhnout po HTTPS z internetu, přes HTTP z telefonu v síti nebo po částech přes BLE.",
          },
        ],
      },
      opensource: {
        title: "Open source ekosystém",
        intro:
          "Software na GitHubu si můžeš prohlédnout a upravit podle licencí. Hardware zůstává u autorů.",
        blocks: [
          {
            icon: feIcon(ic.branch),
            heading: "Firmware a aplikace veřejně",
            text:
              "Zdrojáky firmware a klienta jsou v repozitáři. Jak přesně je smíš použít, říká licence v projektu.",
          },
          {
            icon: feIcon(ic.book),
            heading: "Školy a kluby",
            text:
              "Můžeš přidat vlastní výukové texty, překlady nebo fork pro experimenty, aniž bys čekal na centrální roadmapu (máš-li kapacitu vývoje).",
          },
          {
            icon: feIcon(ic.shield),
            heading: "Jak přispět zpět",
            text:
              "Větší změny projdou review. Issue nebo pull request pomůže stejně dalším učitelům a vývojářům v komunitě.",
          },
        ],
      },
    };

    function renderFeatureBlocks(blocks) {
      return blocks
        .map(function (b) {
          var logoHtml = "";
          if (b.logo) {
            logoHtml =
              '<div class="feature-detail-block__logo"><img src="' +
              b.logo.src +
              '" alt="' +
              b.logo.alt +
              '" width="200" height="80" loading="lazy" decoding="async"></div>';
          }
          var figHtml = "";
          if (b.img) {
            figHtml =
              '<figure class="feature-detail-block__figure"><img src="' +
              b.img.src +
              '" alt="' +
              b.img.alt +
              '" loading="lazy" decoding="async"></figure>';
          }
          var iconHtml = b.icon || "";
          return (
            '<section class="feature-detail-block"><div class="feature-detail-block__head">' +
            iconHtml +
            "<div><h3>" +
            b.heading +
            "</h3></div></div>" +
            logoHtml +
            figHtml +
            '<p class="feature-detail-block__text">' +
            b.text +
            "</p></section>"
          );
        })
        .join("");
    }

    var featureDlg = document.getElementById("feature-detail-dialog");
    var featureAnim = bindAnimatedModal(
      featureDlg && typeof featureDlg.showModal === "function"
        ? featureDlg
        : null
    );
    var featureTitle = document.getElementById("feature-detail-title");
    var featureBody = document.getElementById("feature-detail-body");
    var featureClose = document.querySelector("[data-close-feature-detail]");
    var featureTiles = document.querySelectorAll("[data-feature-detail]");

    function setFeatureTilesExpanded(active) {
      featureTiles.forEach(function (t) {
        t.setAttribute(
          "aria-expanded",
          active === t ? "true" : "false"
        );
      });
    }

    function openFeatureDetail(key, trigger) {
      var data = FEATURE_PAGES[key];
      if (!data || !featureDlg || !featureTitle || !featureBody) return;
      featureTitle.textContent = data.title;
      featureBody.innerHTML =
        (data.intro
          ? '<p class="feature-detail-modal__intro">' + data.intro + "</p>"
          : "") + renderFeatureBlocks(data.blocks);
      setFeatureTilesExpanded(trigger || null);
      if (typeof featureDlg.showModal === "function") {
        featureAnim.open();
      } else {
        featureDlg.setAttribute("open", "");
        featureDlg.style.display = "block";
        requestAnimationFrame(function () {
          requestAnimationFrame(function () {
            featureDlg.classList.add("cz-modal--visible");
          });
        });
      }
    }

    function closeFeatureDetail() {
      if (!featureDlg) return;
      if (
        typeof featureDlg.showModal === "function" &&
        typeof featureDlg.close === "function" &&
        featureDlg.open
      ) {
        featureAnim.close();
      } else {
        featureDlg.classList.remove("cz-modal--visible");
        featureDlg.removeAttribute("open");
        featureDlg.style.display = "none";
      }
      setFeatureTilesExpanded(null);
    }

    if (featureDlg && featureTiles.length) {
      featureDlg.addEventListener("close", function () {
        setFeatureTilesExpanded(null);
      });
      featureDlg.addEventListener("click", function (ev) {
        if (ev.target === featureDlg) {
          closeFeatureDetail();
        }
      });
      if (featureClose) {
        featureClose.addEventListener("click", closeFeatureDetail);
      }
      featureTiles.forEach(function (tile) {
        tile.addEventListener("click", function () {
          var key = tile.getAttribute("data-feature-detail");
          if (key) openFeatureDetail(key, tile);
        });
        tile.addEventListener("keydown", function (ev) {
          if (ev.key === "Enter" || ev.key === " ") {
            ev.preventDefault();
            var key = tile.getAttribute("data-feature-detail");
            if (key) openFeatureDetail(key, tile);
          }
        });
      });
    }
  })();

  /* Scroll reveal (fade-up při vstupu do viewportu; bez JS / reduced motion = obsah viditelný hned) */
  (function initScrollReveal() {
    var nodes = document.querySelectorAll(".reveal, .reveal-stagger");
    if (!nodes.length) return;

    function showAll() {
      nodes.forEach(function (el) {
        el.classList.add("is-visible");
      });
    }

    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      showAll();
      return;
    }

    if (!("IntersectionObserver" in window)) {
      showAll();
      return;
    }

    document.documentElement.classList.add("js-scroll-reveal");

    var io = new IntersectionObserver(
      function (entries) {
        entries.forEach(function (entry) {
          if (entry.isIntersecting) {
            entry.target.classList.add("is-visible");
            io.unobserve(entry.target);
          }
        });
      },
      { root: null, rootMargin: "0px 0px -7% 0px", threshold: 0.07 }
    );

    nodes.forEach(function (el) {
      io.observe(el);
    });
  })();
})();
