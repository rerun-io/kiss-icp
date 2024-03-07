let
  pkgs = import <nixpkgs> {};
in
  pkgs.mkShell rec {
    packages = with pkgs; [
      nixpkgs-fmt
      glxinfo
      python311
    ];
    buildInputs = with pkgs; [
      arrow-cpp
      pkg-config
      libxkbcommon
      libGL
      glib
      libudev-zero
      freetype
      gcc-unwrapped
      pkgs.vulkan-loader
      xorg.libX11
      xorg.libXcursor
      xorg.libXrender
      xorg.libXi
      xorg.libXrandr
      xorg.libxcb
    ];
    LD_LIBRARY_PATH= pkgs.lib.makeLibraryPath buildInputs;
  }