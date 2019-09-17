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

  devenv = stdenv.mkDerivation {
    name = "dev-environment"; # Probably put a more meaningful name here
    buildInputs = [ 
        gnome2.gtk 
        python27Packages.python 
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
      # lilv 
      lv2
      makeWrapper 
      pango 
      perl 
      pkgconfig 
      python2 
      rubberband 
      serd 
      sord
      # sratom 
      # suil 
      taglib 
      vampSDK 
      libarchive
    ];
  };

 in devenv
