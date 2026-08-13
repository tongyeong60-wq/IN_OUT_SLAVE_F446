# IN_OUT_SLAVE_F446 작업 지침

## 프로젝트 목적

이 프로젝트는 기존 STM32F103 기반 `IN_OUT_SLAVE`의 애플리케이션 기능을 STM32F446RE 기반 신규 IO 보드로 이식하는 프로젝트다.

## 필수 작업 절차

모든 코드 및 설정 변경 작업은 반드시 다음 순서를 따른다.

### 1. 분석

- 관련 파일과 함수를 먼저 확인한다.
- 현재 동작과 변경 영향 범위를 설명한다.
- 이 단계에서는 어떤 파일도 수정하지 않는다.

### 2. 수정 계획

- 수정 대상 파일과 함수를 명시한다.
- 변경 방법과 위험요소를 설명한다.
- 이 단계에서도 어떤 파일도 수정하지 않는다.

### 3. 사용자 승인

- 수정 계획을 보고한 뒤 반드시 사용자 승인을 기다린다.
- 사용자의 명시적인 승인 전에는 코드나 설정을 수정하지 않는다.

### 4. 실제 수정

- 사용자가 승인한 범위만 수정한다.
- 최소 변경 원칙을 따른다.
- 요청하지 않은 변경이나 리팩터링을 포함하지 않는다.

### 5. Diff 검토

- `git diff`로 변경사항을 확인한다.
- 의도하지 않은 변경이 없는지 확인한다.

### 6. 빌드

- STM32CubeIDE 프로젝트 기준으로 빌드한다.
- Error와 Warning을 구분하여 보고한다.

### 7. 결과 보고

다음 항목을 보고한다.

- 수정 파일
- 수정 함수
- 변경 이유
- diff 요약
- 빌드 결과
- 실기 테스트 필요 항목

## 하드웨어 기준

### MCU 및 클럭

- MCU: STM32F446RET6
- SYSCLK: 84 MHz
- HCLK: 84 MHz
- PCLK1: 42 MHz
- PCLK2: 84 MHz

### RS485

- Peripheral: USART1
- PA9: USART1_TX
- PA10: USART1_RX
- PA8: DE_485
- 통신 설정: 115200, 8 data bits, no parity, 1 stop bit (8N1)
- USART1 RX interrupt 사용

### Debug / Tera Term

- Peripheral: USART2
- PA2: USART2_TX
- PA3: USART2_RX
- 통신 설정: 115200, 8 data bits, no parity, 1 stop bit (8N1)

### DIP 주소 입력

- DIP0: PC0
- DIP1: PC1
- DIP2: PC2
- DIP3: PC3

### 입력 5점

- POTO1: PC4
- POTO2: PC5
- POTO3: PC6
- POTO4: PC7
- POTO5: PC8

### 출력 5점

- OUT1: PB6
- OUT2: PB7
- OUT3: PB8
- OUT4: PB9
- OUT5: PB10

회로도의 PWM1~PWM5는 소프트웨어의 OUT1~OUT5와 대응한다. 현재 출력은 Timer PWM이 아니라 일반 GPIO ON/OFF 출력이다.

### 7SEG

- LOAD: PB13
- SCLK: PB14
- SDI: PB15
- GPIO bit-banging 방식이며 SPI peripheral을 사용하지 않는다.

### SWD

- PA13: SWDIO
- PA14: SWCLK

## 중요 규칙

- `.ioc`와 CubeMX 설정은 사용자 승인 없이 변경하지 않는다.
- CubeMX가 생성한 F446용 `main.c`, `main.h`, HAL MSP, startup, Drivers를 기존 F103 파일로 덮어쓰지 않는다.
- 기존 F103 프로젝트의 `main.c`, `main.h`, Drivers, Startup을 직접 복사하지 않는다.
- 기존 F103 프로젝트에서는 애플리케이션 기능만 분석하여 F446에 이식한다.
- 기존 RS485 프로토콜과 MASTER 호환성을 유지한다.
- 입력은 신규 하드웨어 기준 5점으로 구현한다.
- 출력은 5점을 유지한다.
- 출력 Active High/Low 극성은 기존 펌웨어와 신규 회로를 비교한 후 결정하며 추측하지 않는다.
- DIP 주소 구조를 임의로 변경하지 않는다.
- `malloc`/`free`를 사용하지 않는다.
- 요청하지 않은 리팩터링을 하지 않는다.
- 빌드 성공만으로 실기 동작 성공이라고 판단하지 않는다.
- RS485, 입력, 출력, DIP, 7SEG는 실기 테스트가 필요하다고 별도로 보고한다.
- `git reset`, `git restore`, `git checkout`, `git clean`은 사용자 승인 없이 실행하지 않는다.
- `commit`, `push`, `pull`, `merge`, `rebase`는 사용자가 명시적으로 요청한 경우에만 수행한다.
