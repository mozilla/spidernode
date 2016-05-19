class Llvm38 < Formula
  desc "llvm 3.8 with asan"
  homepage "http://llvm.org/"
  url "https://people.mozilla.org/~eakhgari/llvm38asan-3.8.0.bottle.tar.gz"
  version "3.8.0"
  sha256 "e42d4a93d32ca982a9118636b430f7ab020b3ab7ecbb4faa431fefcfec13c62e"

  def install
    bin.install Dir["3.8.0/bin/*"]
    lib.install Dir["3.8.0/lib/*"]
    share.install Dir["3.8.0/share/*"]
  end
end
