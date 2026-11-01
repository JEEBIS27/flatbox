let
  pkgs = import (builtins.fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/archive/f3a73bcf5d1c5b75a66cf35a48957e30d41d647a.tar.gz";
    sha256 = "sha256-qVt97ILUu2DorFGGEi+mqVuzOF8R18fQLgFkHiLrNfE=";
  }) {};
in
pkgs.mkShell {
  nativeBuildInputs = with pkgs; [ cmake ninja gcc gcc-arm-embedded git python3 pkg-config ];
}
