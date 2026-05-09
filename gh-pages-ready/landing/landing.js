(function () {
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

  /* Na file:// embed YouTube často spadne (Error 153); zobrazíme jen náhled bez odkazu na YouTube. */
  (function initHeroYoutubeFileFallback() {
    var ytFrame = document.querySelector(".hero__video-frame[data-youtube-id]");
    if (!ytFrame || window.location.protocol !== "file:") return;
    var vid = ytFrame.getAttribute("data-youtube-id");
    if (!vid || !/^[\w-]{11}$/.test(vid)) return;
    ytFrame.innerHTML =
      '<div class="hero__video-fallback">' +
      '<img class="hero__video-fallback__img" src="https://img.youtube.com/vi/' +
      encodeURIComponent(vid) +
      '/maxresdefault.jpg" alt="Náhled představujícího videa CzechMate" width="1280" height="720" loading="lazy" decoding="async" onerror="this.onerror=null;this.src=\'https://img.youtube.com/vi/' +
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
    if (!apkAnchors.length && !dmgAnchors.length) return;
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
      })
      .catch(function () {
        /* Fallback zůstane href z HTML (stránka release) */
      });
  })();

  if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
    document.querySelectorAll("video[data-hero-video]").forEach(function (v) {
      v.removeAttribute("autoplay");
      try {
        v.pause();
      } catch (e) {}
    });
  }

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
            img: {
              src: "landing/assets/board-render.webp",
              alt: "Vizualizace šachovnice CzechMate s podsvícenými poli",
            },
          },
          {
            icon: feIcon(ic.layers),
            heading: "Jedna partie, dvě obrazovky",
            text:
              "Historie tahů a výukové režimy v aplikaci kopírují stav z desky. Žádný „diagram vedle reality“, který by neodpovídal figurkám.",
            img: {
              src: "landing/assets/app-mock.webp",
              alt: "Obrazovka aplikace CzechMate s přehledem partie",
            },
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
              "Na desce běží HTTP server. Výuku, nastavení i hru otevřeš z adresy šachovnice v prohlížeči. Hodí se na ukázku nebo na zařízení bez aplikace.",
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
