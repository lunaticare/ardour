# default.nix
with import <nixpkgs> {};

let
  frameworks = pkgs.darwin.apple_sdk.frameworks;

  aubio = stdenv.mkDerivation rec {
    name = "aubio-0.4.9";

    src = fetchurl {
      url = "https://aubio.org/pub/${name}.tar.bz2";
      sha256 = "1npks71ljc48w6858l9bq30kaf5nph8z0v61jkfb70xb9np850nl";
    };

    nativeBuildInputs = [ pkgconfig python wafHook ];
    buildInputs = [ fftw libjack2 libsamplerate libsndfile frameworks.CoreMedia ];

    meta = with stdenv.lib; {
      description = "Library for audio labelling";
      homepage = https://aubio.org/;
      license = licenses.gpl2;
      maintainers = with maintainers; [ goibhniu marcweber fpletz ];
      platforms = platforms.darwin;
    };
  };

  lv2 = stdenv.mkDerivation rec {
    pname = "lv2";
    version = "1.16.0";

    src = fetchurl {
      url = "http://lv2plug.in/spec/${pname}-${version}.tar.bz2";
      sha256 = "1ppippbpdpv13ibs06b0bixnazwfhiw0d0ja6hx42jnkgdyp5hyy";
    };

    nativeBuildInputs = [ pkgconfig  ];
    buildInputs = [ gtk2 libsndfile python ];

    # wafFlags = "--no-plugins --prefix=$(out) --lv2dir=$(out)";
    # wafFlags = [
    #   "--no-plugins" 
    #   "--lv2dir=$out"
    # ];
    # wafConfigureFlags = [
    #   "--no-plugins" 
    #   "--lv2dir=${out}/bin" 
    # ];

    # configurePhase = ''
    # echo 123
    # '';

    # buildPhase = ''
    # echo 123
    # '';

    installPhase = ''
    ./waf configure build --no-plugins --prefix="$out" --lv2dir="$out"
    ./waf install
    '';

    meta = with stdenv.lib; {
      homepage = http://lv2plug.in;
      description = "A plugin standard for audio systems";
      license = licenses.mit;
      maintainers = [ maintainers.goibhniu ];
      platforms = platforms.darwin;
    };
  };

  sratom = stdenv.mkDerivation rec {
    pname = "sratom";
    version = "0.6.2";

    src = fetchurl {
      url = "https://download.drobilla.net/${pname}-${version}.tar.bz2";
      sha256 = "0lz883ravxjf7r9wwbx2gx9m8vhyiavxrl9jdxfppjxnsralll8a";
    };

    nativeBuildInputs = [ pkgconfig wafHook ];
    buildInputs = [ lv2 python serd sord ];

    meta = with stdenv.lib; {
      homepage = http://drobilla.net/software/sratom;
      description = "A library for serialising LV2 atoms to/from RDF";
      license = licenses.mit;
      maintainers = [ maintainers.goibhniu ];
      platforms = platforms.linux;
    };
  };

  lilv = stdenv.mkDerivation rec {
    pname = "lilv";
    version = "0.24.4";

    src = fetchurl {
      url = "https://download.drobilla.net/${pname}-${version}.tar.bz2";
      sha256 = "0f24cd7wkk5l969857g2ydz2kjjrkvvddg1g87xzzs78lsvq8fy3";
    };

    nativeBuildInputs = [ pkgconfig wafHook ];
    buildInputs = [ lv2 python serd sord sratom ];

    meta = with stdenv.lib; {
      homepage = http://drobilla.net/software/lilv;
      description = "A C library to make the use of LV2 plugins";
      license = licenses.mit;
      maintainers = [ maintainers.goibhniu ];
      platforms = platforms.linux;
    };
  };

  vampSDK = stdenv.mkDerivation {
    name = "vamp-sdk-2.7.1";
    # version = "2.7.1";

    src = fetchFromGitHub {
      owner = "c4dm";
      repo = "vamp-plugin-sdk";
      rev = "vamp-plugin-sdk-v2.7.1";
      sha256 = "1ifd6l6b89pg83ss4gld5i72fr0cczjnl2by44z5jnndsg3sklw4";
    };

    nativeBuildInputs = [ pkgconfig ];
    buildInputs = [ libsndfile ];

    meta = with stdenv.lib; {
      description = "Audio processing plugin system for plugins that extract descriptive information from audio data";
      homepage = https://sourceforge.net/projects/vamp;
      license = licenses.bsd3;
      maintainers = [ maintainers.goibhniu maintainers.marcweber ];
      platforms = [ platforms.linux platforms.darwin ];
    };
  };

  rubberband = stdenv.mkDerivation {
    name = "rubberband-1.8.2";

    src = fetchurl {
      url = https://github.com/breakfastquay/rubberband/archive/v1.8.2.tar.gz;
      sha256 = "058x04m66c8fy2981jg3x3lkfqlc2z0y3czn1i414rs71dqmic0b";
    };

    nativeBuildInputs = [ 
      pkgconfig
    ];

    buildInputs = [
      libsamplerate 
      libsndfile 
      fftw 
      vampSDK 
      ladspaH
      frameworks.CoreMedia
    ];

    # buildPhase = ''
    # make -f Makefile.osx
    # '';
    preBuild = ''
     substitute Makefile.osx Makefile.osx.p \
        --replace 'cp $(VAMP_TARGET) $(DESTDIR)$(INSTALL_VAMPDIR)' "" \
        --replace 'cp vamp/vamp-rubberband.cat $(DESTDIR)$(INSTALL_VAMPDIR)' "" \
        --replace 'cp $(LADSPA_TARGET) $(DESTDIR)$(INSTALL_LADSPADIR)' "" \
        --replace 'cp ladspa/ladspa-rubberband.cat $(DESTDIR)$(INSTALL_LADSPADIR)' "" \
        --replace 'cp ladspa/ladspa-rubberband.rdf $(DESTDIR)$(INSTALL_LRDFDIR)' ""
    '';
    makefile = "Makefile.osx.p";
    makeFlags = [ "PREFIX=$(out)" ];

    meta = with stdenv.lib; {
      description = "High quality software library for audio time-stretching and pitch-shifting";
      homepage = https://www.breakfastquay.com/rubberband/index.html;
      # commercial license available as well, see homepage. You'll get some more optimized routines
      license = licenses.gpl2Plus;
      maintainers = [ maintainers.goibhniu maintainers.marcweber ];
      platforms = [ platforms.darwin ];
    };
  };

  tag = "5.12";

  devenv = stdenv.mkDerivation {
    name = "dev-environment"; # Probably put a more meaningful name here
    src = fetchgit {
      url = "git://git.ardour.org/ardour/ardour.git";
      rev = "ae0dcdc0c5d13483271065c360e378202d20170a";
      sha256 = "0mla5lm51ryikc2rrk53max2m7a5ds6i1ai921l2h95wrha45nkr";
    };

    nativeBuildInputs = [ 
      pkgconfig 
      python27Packages.python 
      wafHook
    ];

    buildInputs = [ 
      gnome2.gtk 
      boost 
      glibmm 
      clang
      pkgconfig 
      libsndfile.dev 
      curl
      libarchive
      liblo
      taglib
      vampSDK
      rubberband
      fftwFloat
      aubio
      # alsaLib
      # aubio 
      boost 
      cairomm 
      curl 
      doxygen 
      dbus 
      fftw 
      fftwSinglePrec 
      flac
      glibmm 
      graphviz 
      gtkmm2 
      libjack2 
      # libgnomecanvas 
      # libgnomecanvasmm
       liblo
      libmad 
      libogg 
      librdf 
      librdf_raptor 
      librdf_rasqal 
      libsamplerate
      libsigcxx 
      libsndfile 
      libusb 
      libuuid 
      libxml2 
      libxslt 
      lilv 
      lv2
      makeWrapper 
      pango 
      perl 
      pkgconfig 
      python2 
      rubberband 
      serd 
      sord
      sratom 
      # suil 
      taglib 
      vampSDK 
      libarchive
      frameworks.AppKit
      frameworks.CoreMedia 
      frameworks.AudioUnit
      frameworks.Accelerate
      frameworks.AudioToolbox
      frameworks.CoreServices 
      frameworks.CoreFoundation
      frameworks.Carbon
      frameworks.Cocoa
      wafHook
    ];

    propagatedBuildInputs = stdenv.lib.optionals stdenv.isDarwin [ ];

    patchPhase = ''
      printf '#include "libs/ardour/ardour/revision.h"\nnamespace ARDOUR { const char* revision = \"${tag}\"; }\n' > libs/ardour/revision.cc
      patchShebangs ./tools/
    '';
  
    wafConfigureFlags = [
      "--strict" 
      "--ptformat" 
      "--with-backends=jack,coreaudio,dummy"
      "--optimize" 
      "--freebie"
    ];
  };

 in devenv
