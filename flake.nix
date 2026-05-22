{
  description = "A flake for building the lispy compiler";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs =
    { nixpkgs, ... }:
    let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;
      llvmPkgs = pkgs.llvmPackages_22;
    in
    {
      devShell.x86_64-linux = pkgs.mkShell {
        nativeBuildInputs = [
          llvmPkgs.clang-tools
          llvmPkgs.clang
          llvmPkgs.libllvm
          llvmPkgs.lldb
        ];
        packages = with pkgs; [
          cmake
          ninja
          pkg-config
          readline
        ];
      };
    };
}
