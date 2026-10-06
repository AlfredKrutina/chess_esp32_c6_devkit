(function () {
  /* Always scroll to top on reload (except hash links); no jump while media loads. */
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

  /** Fade + light dialog/backdrop shift; Escape via `cancel` + preventDefault */
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

  /** Video spinner: is-loading → is-ready after canplay / preview. */
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
      // Do not strip HTML posters. The board film opens on black glass; without
      // a poster the card shows an empty frame once metadata loads.
      v.controls = false;
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

  /* HLS VOD: 2 s fMP4 chunks, ABR 720/1080. Safari uses native HLS; others hls.js. */
  var czHls = (function () {
    function lite() {
      if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) return true;
      try {
        var c = navigator.connection || navigator.mozConnection || navigator.webkitConnection;
        if (c && (c.saveData || c.effectiveType === "slow-2g" || c.effectiveType === "2g")) return true;
      } catch (e) {}
      return false;
    }

    function nativeHls(video) {
      return video.canPlayType("application/vnd.apple.mpegurl") === "probably"
        || video.canPlayType("application/vnd.apple.mpegurl") === "maybe";
    }

    function attach(video, opts) {
      opts = opts || {};
      var master = video.getAttribute("data-hls");
      var fallback = video.getAttribute("data-hls-fallback");
      if (!master) return false;
      if (video.dataset.hlsBound === "1") return true;
      video.dataset.hlsBound = "1";

      if (lite() && fallback) {
        video.src = fallback;
        return true;
      }

      var useNative = nativeHls(video) && !(window.Hls && window.Hls.isSupported() && /Chrome|Chromium|Edg\//.test(navigator.userAgent));
      if (useNative && nativeHls(video) && !window.Hls) {
        video.src = master;
        return true;
      }
      if (window.Hls && window.Hls.isSupported()) {
        var hls = new window.Hls({
          enableWorker: true,
          lowLatencyMode: false,
          maxBufferLength: opts.hero ? 12 : 8,
          maxMaxBufferLength: opts.hero ? 24 : 14,
          startLevel: -1,
          abrEwmaDefaultEstimate: 2.2e6,
          testBandwidth: true,
          autoStartLoad: opts.autoStart !== false,
        });
        hls.loadSource(master);
        hls.attachMedia(video);
        video._hls = hls;
        hls.on(window.Hls.Events.ERROR, function (_ev, data) {
          if (!data || !data.fatal || !fallback) return;
          try {
            hls.destroy();
          } catch (eD) {}
          video.src = fallback;
        });
        return true;
      }
      if (nativeHls(video)) {
        video.src = master;
        return true;
      }
      if (fallback) {
        video.src = fallback;
        return true;
      }
      return false;
    }

    function start(video) {
      if (video._hls && video._hls.startLoad) video._hls.startLoad();
    }

    function stopBuffer(video) {
      if (video._hls && video._hls.stopLoad) video._hls.stopLoad();
    }

    return { attach: attach, start: start, stopBuffer: stopBuffer, lite: lite };
  })();

  /* Hero reel: full presentation through mate/outro; soft-loop from board wake-up. */
  (function initHeroReelLoad() {
    var v = document.getElementById("czm-hero-reel");
    if (!v) return;
    var media = v.closest("[data-video-media]");
    // Skip the dark chassis wall on replay so the story restarts on the living board.
    var LOOP_AT = 7.0;
    v.setAttribute("playsinline", "");
    v.muted = true;

    function reveal() {
      if (media) czVideoMedia.markReady(media);
    }

    function kick() {
      if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
        reveal();
        return;
      }
      czHls.attach(v, { hero: true, autoStart: true });
      v.preload = "auto";
      var pr = v.play();
      if (pr && typeof pr.catch === "function") {
        pr.catch(function () {});
      }
    }

    v.addEventListener("ended", function () {
      try {
        v.currentTime = LOOP_AT;
      } catch (eL) {
        v.currentTime = 0;
      }
      var again = v.play();
      if (again && typeof again.catch === "function") {
        again.catch(function () {});
      }
    });

    v.addEventListener("loadeddata", reveal, { once: true });
    v.addEventListener("canplay", reveal, { once: true });
    v.addEventListener("playing", reveal, { once: true });

    // Wait for CSS/poster paint, then idle if available so other critical assets win.
    window.requestAnimationFrame(function () {
      window.requestAnimationFrame(function () {
        if (typeof window.requestIdleCallback === "function") {
          window.requestIdleCallback(kick, { timeout: 400 });
        } else {
          window.setTimeout(kick, 120);
        }
      });
    });
  })();

  /* Welcome MP4: start download as soon as the script runs. */
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

  /* Direct APK/DMG download from the latest GitHub release (API → browser_download_url) */
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
        /* Fallback keeps the HTML href (release page) */
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

  /* #czm-v2-welcome-video: scroll → play; no hover; click → replay from start; no controls bar. */
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

  /* MP4: lazy + staggered download starts (two files do not begin at once). */
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
      v.dataset.lazyLocalHydrated = "1";
      if (v.getAttribute("data-hls")) {
        czHls.attach(v, { autoStart: true });
      } else {
        v.preload = "auto";
        v.querySelectorAll("source[data-src]").forEach(function (s) {
          var url = s.getAttribute("data-src");
          if (!url) return;
          s.src = url;
          s.removeAttribute("data-src");
        });
        try {
          v.load();
        } catch (eL) {}
      }
      czVideoMedia.bindVideo(v);
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

    /* Preview #ai-coach-app-preview sits above #app-phone-demo-video — load MP4 before scrolling to the player. */
    function bindAiKoucEarlyAppVideoLoad() {
      var appDemo = document.getElementById("app-phone-demo-video");
      var aiKoucSection = document.getElementById("ai-coach");
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

  /* #ai-coach-app-preview: frame from MP4 at data-ai-preview-at (0–1, default 0.52 — end is often black/fade). */
  (function initAiKoucVideoPreviewFrame() {
    var demo = document.getElementById("app-phone-demo-video");
    var preview = document.getElementById("ai-coach-app-preview");
    var aiKoucSection = document.getElementById("ai-coach");
    var reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

    if (!demo || !preview) return;

    var done = false;
    var scheduleTimer = null;

    function ensureDemoSourceForCapture() {
      if (demo.dataset.lazyLocalHydrated === "1") return;
      demo.dataset.lazyLocalHydrated = "1";
      if (demo.getAttribute("data-hls")) {
        czHls.attach(demo, { autoStart: true });
        return;
      }
      demo.querySelectorAll("source[data-src]").forEach(function (s) {
        var url = s.getAttribute("data-src");
        if (!url) return;
        s.src = url;
        s.removeAttribute("data-src");
      });
      try {
        demo.load();
      } catch (eH) {}
    }

    /* Fallback: if the lazy-loader has not run yet, start the source as the AI coach section nears. */
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
            preview.alt = "CzechMate — AI coach in the app";
            done = true;
            var previewMedia = preview.closest("[data-video-media]");
            if (previewMedia) czVideoMedia.markReady(previewMedia);
          }
        } catch (e1) {
          /* e.g. security context / corrupted source */
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

  /* Muted product films: play when sufficiently visible, pause when not. */
  (function initPlayWhenVisibleVideos() {
    var reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    document.querySelectorAll("video[data-play-when-visible]").forEach(function (v) {
      // Welcome has its own thresholds and click-to-replay.
      if (v.id === "czm-v2-welcome-video") return;
      v.removeAttribute("autoplay");
      v.controls = false;
      if (reduce) {
        try {
          v.pause();
        } catch (e0) {}
        return;
      }
      if (!("IntersectionObserver" in window)) return;

      function ensureMediaSource(vid) {
        if (vid.dataset.lazyLocalHydrated === "1") return;
        vid.dataset.lazyLocalHydrated = "1";
        if (vid.getAttribute("data-hls")) {
          czHls.attach(vid, { autoStart: true });
          return;
        }
        vid.querySelectorAll("source[data-src]").forEach(function (s) {
          var url = s.getAttribute("data-src");
          if (!url) return;
          s.src = url;
          s.removeAttribute("data-src");
        });
        try {
          vid.load();
        } catch (eL) {}
      }

      function tryPlay() {
        ensureMediaSource(v);
        if (v.ended) {
          try {
            v.currentTime = 0;
          } catch (eR) {}
        }
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
    });
  })();

  /* Ambient product loops: hero reel + LED + film cards — no pause, no native UI. */
  (function initAmbientLoopsNoPause() {
    document.querySelectorAll("video[data-no-pause]").forEach(function (v) {
      v.controls = false;

      function blockNativeVideoUi(ev) {
        ev.preventDefault();
      }
      v.addEventListener("click", blockNativeVideoUi);
      v.addEventListener("dblclick", blockNativeVideoUi);
      v.addEventListener("contextmenu", blockNativeVideoUi);

      var reduceMq = window.matchMedia("(prefers-reduced-motion: reduce)");
      var holdSec = parseFloat(v.getAttribute("data-loop-hold"));
      var holdMs = isFinite(holdSec) && holdSec > 0 ? holdSec * 1000 : 0;
      if (holdMs) {
        v.removeAttribute("loop");
        v.loop = false;
      }

      function resumeIfAllowed() {
        if (reduceMq.matches) return;
        if (v._holdLoop) return;
        if (v.ended || !v.paused) return;
        var pr = v.play();
        if (pr && typeof pr.catch === "function") {
          pr.catch(function () {});
        }
      }

      v.addEventListener("pause", resumeIfAllowed);
      v.addEventListener("ended", function () {
        if (reduceMq.matches) return;
        if (holdMs) {
          v._holdLoop = true;
          try {
            v.pause();
          } catch (eH) {}
          window.setTimeout(function () {
            v._holdLoop = false;
            try {
              v.currentTime = 0;
            } catch (eS) {}
            var pr = v.play();
            if (pr && typeof pr.catch === "function") {
              pr.catch(function () {});
            }
          }, holdMs);
          return;
        }
        try {
          v.currentTime = 0;
        } catch (eZ) {}
        var replay = v.play();
        if (replay && typeof replay.catch === "function") {
          replay.catch(function () {});
        }
      });
      document.addEventListener("visibilitychange", function () {
        if (!document.hidden) resumeIfAllowed();
      });
      resumeIfAllowed();
    });
  })();

  /* Film cards: click to play/pause, never show the native control bar. */
  (function initClickPlayFilms() {
    document.querySelectorAll("video[data-click-play]").forEach(function (v) {
      v.controls = false;
      v.setAttribute("playsinline", "");
      v.addEventListener("click", function () {
        if (v.paused || v.ended) {
          if (v.ended) {
            try {
              v.currentTime = 0;
            } catch (eR) {}
          }
          var pr = v.play();
          if (pr && typeof pr.catch === "function") {
            pr.catch(function () {});
          }
        } else {
          v.pause();
        }
      });
    });
  })();

  /* Legacy class: still force controls off if present. */
  (function initVideoControlsOnClick() {
    document.querySelectorAll("video.split__video--controls-on-click").forEach(function (v) {
      v.controls = false;
    });
  })();

  /* Pre-order modal (<dialog>) */
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
    /* Fallback without native <dialog> */
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

  /* Feature details — modal (cards in the App section) */
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
        title: "Hints and learning modes",
        intro:
          "Lights on the squares and the app say the same thing: you do not have to translate the position in your head.",
        blocks: [
          {
            icon: feIcon(ic.sun),
            heading: "What the board shows on the spot",
            text:
              "Hall sensors recognize the piece type on each square. LEDs help with suggested moves, check and mate, promotions, and bringing attention back to the right piece. In the app you get the same position with text and analysis.",
          },
          {
            icon: feIcon(ic.layers),
            heading: "One game, two screens",
            text:
              "Move history and learning modes in the app mirror the board state. No “diagram beside reality” that does not match the pieces.",
          },
          {
            icon: feIcon(ic.book),
            heading: "For beginners and advanced players",
            text:
              "Beginners see clear signals on the board. More advanced players can open deeper analysis or the coach in the app. One system grows with your level without swapping tools.",
          },
        ],
      },
      analysis: {
        title: "Analysis and Stockfish",
        intro:
          "The Stockfish engine adds evaluations and move suggestions; you set strength to your level.",
        blocks: [
          {
            icon: feIcon(ic.gauge),
            heading: "Strong engine, calm lessons",
            text:
              "Stockfish performs above the human ceiling. In CzechMate you get position evaluations and move suggestions, but you can pick a weaker line so it stays about learning, not a streak of one-sided results.",
          },
          {
            icon: feIcon(ic.list),
            heading: "Post-game review and prep",
            text:
              "In the app you get variations, analysis lines, and a comparison with your own move. Useful for preparation and for review after training.",
          },
          {
            icon: feIcon(ic.cloud),
            heading: "Same logic from a browser",
            text:
              "With Wi‑Fi and the board web UI you can run part of a lesson or a game against the engine from a browser. The game runs the same; only the client changes.",
          },
        ],
      },
      stockfish_a: {
        title: "Games stronger than any human",
        intro:
          "An opponent stronger than any living player, but tunable so the game still teaches you something.",
        blocks: [
          {
            icon: feIcon(ic.gauge),
            heading: "Engine strength under control",
            text:
              "Full-strength Stockfish beats every living player. For learning you pick a weaker line and a softer pace so you can read the game, not just lose.",
          },
          {
            icon: feIcon(ic.list),
            heading: "What you get while learning",
            text:
              "On each move you see the gap between your choice and the engine suggestion. For practice that context often matters more than the game result.",
          },
        ],
      },
      stockfish_b: {
        title: "Rough evaluation",
        intro:
          "A computed score helps you read the imbalance — best alongside variations in analysis.",
        blocks: [
          {
            icon: feIcon(ic.gauge),
            heading: "Numbers as a guide, not a verdict",
            text:
              "A rough score shows the direction of advantage on the board. It matters most in the middlegame and endgames, where small details grow over several moves.",
          },
          {
            icon: feIcon(ic.list),
            heading: "Always with move context",
            text:
              "Evaluation makes sense next to variations and analysis lines. A bare number without a multi-move plan rarely helps enough.",
          },
        ],
      },
      stockfish_c: {
        title: "Board ↔ app linked",
        intro:
          "Firmware holds the truth about the position; board, web, and app only read the same state.",
        blocks: [
          {
            icon: feIcon(ic.layers),
            heading: "One position everywhere",
            text:
              "LEDs, the web UI, and the phone show the same thing. Hall sensors know which piece stands where, so lessons on the lights and text on the display match the real chessboard.",
          },
          {
            icon: feIcon(ic.sync),
            heading: "Bluetooth or network",
            text:
              "Nearby, BLE is often enough. At home or school you can also use Wi‑Fi and HTTP or WebSocket where firmware offers it. You change the client, not the game rules.",
          },
        ],
      },
      coach: {
        title: "AI coach / chat",
        intro:
          "Questions on the current position: plan, mistakes, and strategy. How deep the explanation goes depends on your settings.",
        blocks: [
          {
            icon: feIcon(ic.chat),
            heading: "Conversation at the board",
            text:
              "The coach responds to plans, mistakes, and general questions. Set the explanation style from classroom-calm to club-level depth.",
          },
          {
            icon: feIcon(ic.cloud),
            heading: "Assistant for your environment",
            text:
              "In settings you can connect cloud or a local option that fits school or home rules. Offline, basic offline hints from built-in chess tips remain available.",
          },
          {
            icon: feIcon(ic.shield),
            heading: "School and privacy",
            text:
              "Cloud services follow their own terms and security. In a school it makes sense to decide on a provider and accounts under your rules ahead of time.",
          },
        ],
      },
      connect: {
        title: "Connect to the board",
        intro:
          "In one room, Bluetooth; at home or in class, often Wi‑Fi and a known IP on the network.",
        blocks: [
          {
            icon: feIcon(ic.bluetooth),
            heading: "Bluetooth within reach",
            text:
              "BLE typically covers one room with no IP setup. Games and commands work like other nearby devices.",
          },
          {
            icon: feIcon(ic.wifi),
            heading: "Wi‑Fi on the LAN",
            text:
              "In network client mode the app talks to the board over HTTP or WebSocket where firmware offers it. Common scenes: home, club room, classroom.",
          },
          {
            icon: feIcon(ic.sync),
            heading: "State always from the board",
            text:
              "Truth about the position lives in firmware. LEDs and the app only display the same thing; short mismatches can happen only during board animations.",
          },
        ],
      },
      webui: {
        title: "Board web UI",
        intro:
          "Enter the board IP in a browser and get an interface without installing another program.",
        blocks: [
          {
            icon: feIcon(ic.globe),
            heading: "Browser interface",
            text:
              "An HTTP server runs on the board. Open lessons, settings, and play from the board’s address in a browser — handy in a classroom on devices without the app installed.",
          },
          {
            icon: feIcon(ic.wifi),
            heading: "Same lessons with or without a cable",
            text:
              "On a stable network, engine play and lessons from the browser work much like the mobile app.",
          },
          {
            icon: feIcon(ic.layers),
            heading: "Web and mobile on the same API",
            text:
              "Phone, web, and custom tools can use the same REST API. WebSocket is available where firmware exposes it.",
          },
        ],
      },
      ota: {
        title: "Flash from the app",
        intro:
          "Upload board firmware from the app with no programmer and no cables to the chip.",
        blocks: [
          {
            icon: feIcon(ic.download),
            heading: "Updates without a lab bench",
            text:
              "New firmware from the app over Wi‑Fi, or in some flows in chunks over BLE. A normal update needs neither UART nor an external flasher.",
          },
          {
            icon: feIcon(ic.led),
            heading: "Watch progress on the LEDs",
            text:
              "You can follow OTA on the chessboard itself. When it finishes, the board boots into the new flash version.",
          },
          {
            icon: feIcon(ic.chip),
            heading: "Several ways to deliver the file",
            text:
              "Depending on board availability, firmware can download over HTTPS from the internet, over HTTP from a phone on the network, or in chunks over BLE.",
          },
        ],
      },
      opensource: {
        title: "Open-source ecosystem",
        intro:
          "Software on GitHub is yours to inspect and adapt under the licenses. Hardware stays with me.",
        blocks: [
          {
            icon: feIcon(ic.branch),
            heading: "Firmware and app are public",
            text:
              "Firmware and client sources live in the repository. Exact reuse rights are set by the project license.",
          },
          {
            icon: feIcon(ic.book),
            heading: "Schools and clubs",
            text:
              "You can add your own lesson texts, translations, or a fork for experiments without waiting on a central roadmap (if you have development capacity).",
          },
          {
            icon: feIcon(ic.shield),
            heading: "How to contribute back",
            text:
              "Larger changes go through review. An issue or pull request helps other teachers and developers in the community too.",
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

  /* Pieces gallery: toggle button swaps white ↔ black with a 3D stage animation */
  (function initPieceToneToggle() {
    var gallery = document.querySelector("[data-piece-tone]");
    var btn = document.querySelector("[data-piece-tone-toggle]");
    if (!gallery || !btn) return;

    var reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    var blacksHydrated = false;
    var tone = 0;
    var animToken = 0;

    function hydrateBlackPieces() {
      if (blacksHydrated) return;
      blacksHydrated = true;
      gallery.querySelectorAll("img.piece-tone__black[data-src]").forEach(function (img) {
        var url = img.getAttribute("data-src");
        if (!url) return;
        img.src = url;
        img.removeAttribute("data-src");
      });
    }

    function updateAlts(isBlack) {
      gallery.querySelectorAll("img.piece-tone__white").forEach(function (img) {
        var base = (img.getAttribute("alt") || "").replace(/\s*—\s*(white|black) PLA set$/i, "");
        if (!base) base = "Piece";
        img.alt = base + (isBlack ? " — black PLA set" : " — white PLA set");
      });
    }

    function setToneInstant(next) {
      tone = next;
      var isBlack = next >= 0.5;
      gallery.style.setProperty("--piece-tone", next.toFixed(4));
      gallery.classList.toggle("is-black", isBlack);
      btn.setAttribute("aria-pressed", isBlack ? "true" : "false");
      btn.setAttribute("aria-label", isBlack ? "Show white pieces" : "Show black pieces");
      updateAlts(isBlack);
    }

    function animateTone(next) {
      var from = tone;
      var to = next;
      var start = performance.now();
      var dur = 920;
      var token = ++animToken;
      var isBlack = to >= 0.5;

      gallery.classList.toggle("is-black", isBlack);
      btn.setAttribute("aria-pressed", isBlack ? "true" : "false");
      btn.setAttribute("aria-label", isBlack ? "Show white pieces" : "Show black pieces");
      updateAlts(isBlack);

      function frame(now) {
        if (token !== animToken) return;
        var t = Math.max(0, Math.min(1, (now - start) / dur));
        var eased = t * t * (3 - 2 * t);
        tone = from + (to - from) * eased;
        gallery.style.setProperty("--piece-tone", tone.toFixed(4));
        if (t < 1) {
          window.requestAnimationFrame(frame);
        } else {
          tone = to;
        }
      }

      window.requestAnimationFrame(frame);
    }

    if ("IntersectionObserver" in window) {
      var io = new IntersectionObserver(
        function (entries) {
          entries.forEach(function (entry) {
            if (entry.isIntersecting) {
              hydrateBlackPieces();
              io.disconnect();
            }
          });
        },
        { rootMargin: "220px 0px" }
      );
      io.observe(gallery);
    } else {
      hydrateBlackPieces();
    }

    setToneInstant(0);

    btn.addEventListener("click", function () {
      hydrateBlackPieces();
      btn.classList.add("is-pressing");
      window.setTimeout(function () {
        btn.classList.remove("is-pressing");
      }, 160);
      var next = tone >= 0.5 ? 0 : 1;
      if (reduceMotion) setToneInstant(next);
      else animateTone(next);
    });
  })();

  /* Scroll reveal (fade-up on enter viewport; without JS / reduced motion = content visible immediately) */
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
