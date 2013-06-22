#IAMROOT.ORG Kernel스터디10차(ARM)


##1 초간단 git 매뉴얼

###1.1 저장소 내려받기
로컬 디스크에 git저장소를내려받는 명령어입니다.
기본 branch는 `study-3.9.6`입니다.
만약 `study-3.9.6`가 아니라면 `git checkout study-3.9.6`명령으로 이동하세요.
현재 branch는 `git branch`로 알 수 있습니다.

	git clone git@github.com:ygpark/iamroot-linux-arm10c.git
    git clone https://github.com/ygpark/iamroot-linux-arm10c.git

###1.2 서버와 동기화하기 (최신버전 내려받기)

    git pull



##2 레퍼런스 매뉴얼

`Reference/` 디렉토리에 레퍼런스 매뉴얼이 있으니 참고하세요.



##3 분석 히스토리
### 2012-06-22: 리누즈박(박영기)
  - arch/arm/boot/compressed/head.S
    - 몇몇 매크로를 보고 gas의 매크로 사용법을 알아 보았습니다.
    - 'start' 레이블을 분석했습니다.
    - '1'레이블을 분석하다가 마쳤습니다.
