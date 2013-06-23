#소스분석환경 구축하기

##VIM 플러그인 설치하기

###Bundle 다운로드 

    apt-get install vim git
    git clone https://github.com/gmarik/vundle.git ~/.vim/bundle/vundle


###Bundle 설치

vimrc.txt 파일은 Reference/vim/ 디렉토리에 있습니다.

    cp vimrc.txt ~/.vimrc
    vi

자 이제 vi에서 `:BundleInstall` 명령을 내리면 설치가 시작됩니다.


##데이터베이스 생성

    make ARCH=arm tags
    make ARCH=arm cscope

##VIM설정

~/.vimrc를 열어서 방금 만든 tags의 경로를 설정한다.

    set tags+=/path/to/tags

##참조
  - [http://www.iamroot.org/xe/Kernel_10_ARM/104190](http://www.iamroot.org/xe/Kernel_10_ARM/104190)
