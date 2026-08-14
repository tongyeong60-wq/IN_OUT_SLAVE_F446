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

## 확정 하드웨어 및 펌웨어 기준

### MCU 확정 기준

- 실제 PCB에 장착할 MCU는 `STM32F446RET6`이다.
- Package는 LQFP64이며 CubeMX는 `STM32F446RETx` 기준을 유지한다.
- 현재 펌웨어도 STM32F446RE 기준을 유지한다.
- 회로도에 `STM32F446RC` 또는 `STM32F446RC(LQFP64)`라고 표시되어 있더라도 RC 사용 예정으로 해석하지 않는다.
- 회로도의 RC 표기는 추후 회로도와 BOM에서 RET6으로 정정할 단순 문서 표기사항이다.
- RC/RE 차이를 문제점으로 반복 보고하거나 RC 기준으로 CubeMX, linker 또는 펌웨어를 변경하지 않는다.
- 모든 MCU 핀과 주변회로 검토는 `STM32F446RET6`/LQFP64 기준으로 수행한다.

### RS485 확정 하드웨어 방식

- RS485는 MAX3485 계열 2-wire Half-Duplex 방식이다.
- MCU UART TX는 MAX3485 DI에 연결한다.
- MAX3485 RO는 MCU UART RX에 연결한다.
- MAX3485 DE(pin 3)와 `/RE`(pin 2)는 서로 묶어 MCU의 `DE_485` GPIO 하나로 제어한다.
- MAX3485 VCC(pin 8)는 3.3V 전원을 유지한다.
- `/RE`(pin 2)를 3.3V에 고정하는 방식은 최종 RS485 방식으로 사용하지 않는다.

`DE_485` 논리는 다음과 같다.

- `DE_485 LOW`: DE=0, `/RE`=0, Driver OFF, Receiver ON, RX mode
- `DE_485 HIGH`: DE=1, `/RE`=1, Driver ON, Receiver OFF, TX mode
- 정상 idle 및 초기 상태는 LOW/RX이다.
- PA8 `DE_485`는 Initial LOW, Output Push-Pull, MCU Internal Pull-up, Low Speed로 설정한다.
- 외부 10kΩ pull-up과 MCU Internal Pull-up의 병행을 허용한다.
- 송신 직전에만 `DE_485`를 HIGH로 전환한다.
- UART 송신 후 Transmission Complete(TC)를 확인한 다음 `DE_485`를 즉시 LOW/RX로 복귀시킨다.
- `DE_485`를 기본 HIGH로 초기화하지 않는다.

### IO 프로젝트 RS485 기준

- USART1 TX: PA9
- USART1 RX: PA10
- DE_485: PA8
- 통신 설정: 115200, 8N1
- USART1 RX interrupt를 사용한다.
- 회로도, `.ioc`, CubeMX 설정 및 코드를 분석할 때 위 기준과의 일치 여부를 확인한다.

### DE_485 Pull 저항 기준

- DE와 `/RE`가 묶인 `DE_485` net에 10kΩ pull-up을 사용하는 회로 설계를 허용한다.
- MCU GPIO 초기화 시 `DE_485`를 LOW로 설정하여 정상 운전 상태를 RX mode로 만든다.
- 10kΩ pull-up을 자동으로 PCB 오류 또는 필수 수정사항으로 판단하지 않는다.
- pull-down은 reset 구간까지 RX를 보장하기 위한 fail-safe 대안일 수 있으나 현재 프로젝트의 필수 변경사항은 아니다.
- 다음 항목은 실기 검증 대상으로 유지한다.
  - MCU reset/Hi-Z 구간에서 `DE_485`가 HIGH가 될 가능성
  - 전원 투입 또는 reset 순간의 RS485 bus glitch 여부
  - GPIO 초기화 후 `DE_485`가 실제 LOW/RX 상태가 되는지

### IO 출력 확정 기준

- OUT1~OUT5는 MCU 기준 Active High이다.
- OUT1=PB6, OUT2=PB7, OUT3=PB8, OUT4=PB9, OUT5=PB10이다.
- MCU LOW는 외부 출력 OFF이다.
- MCU HIGH는 외부 출력 ON이며 외부 `P_OUT`은 low-side sink 방식으로 GND에 당겨진다.
- 펌웨어는 `OUT_ACTIVE_HIGH = 1`, ON=`GPIO SET`, OFF=`GPIO RESET`을 유지한다.
- 부팅 초기값은 LOW/OFF를 기준으로 한다.
- `io_all_off()`는 반드시 모든 OUT을 LOW로 만든다.

### POTO 입력 확정 기준

- POTO1=PC4, POTO2=PC5, POTO3=PC6, POTO4=PC7, POTO5=PC8이다.
- POTO1~POTO5는 Active Low 입력이다.
- 외부 입력 OFF는 MCU GPIO HIGH이며 논리 OFF=0이다.
- 외부 입력 ON은 MCU GPIO LOW이며 논리 ON=1이다.
- 외부 10kΩ pull-up이 있으므로 CubeMX `GPIO_NOPULL`을 기준으로 한다.
- 펌웨어는 GPIO LOW를 논리 ON으로 변환하고 L1~L5를 모두 지원한다.

### DIP 입력 확정 기준

- DIP0=PC0, DIP1=PC1, DIP2=PC2, DIP3=PC3이다.
- DIP0~DIP3는 외부 10kΩ pull-up이 있는 Active Low 입력이다.
- DIP OFF는 GPIO HIGH이고 DIP ON은 GPIO LOW이다.
- CubeMX `GPIO_NOPULL`을 기준으로 한다.
- 주소 계산식 `addr = (~raw) & 0x0F`를 유지한다.
- 주소 0은 broadcast용으로 취급하고 일반 Slave 운용 주소는 1~15를 기준으로 한다.

### 7SEG 확정 기준

- LOAD=PB13, SCLK=PB14, SDI=PB15이다.
- GPIO bit-banging 방식을 유지하며 SPI peripheral로 임의 변경하지 않는다.
- 외부 7SEG/74HC595 모듈의 common anode/cathode, segment order, digit order 및 LOAD edge는 실기 확인 대상이다.

### USART2 및 Tera Term 기준

- USART2는 디버그 로그용으로 유지한다.
- USART2 TX=PA2, USART2 RX=PA3이다.
- 통신 설정은 115200, 8N1이다.
- 기존 log 구조를 임의 변경하지 않는다.

## 상태머신 및 프로토콜 보존 기준

- 기존 RS485 프레임, CRC, 주소 및 sequence 정책을 유지한다.
- 다음 안정화 동작을 임의로 되돌리거나 단순화하지 않는다.
  - RUN → ACK → STATUS → BUSY → DONE
  - RUN의 K/V 전체 검증 후 ACK
  - R1~R5 지원
  - L1~L5 지원
  - 동일 transaction retry 시 ACK replay
  - BUSY 중 다른 RUN 수신 시 NACK
  - DONE 상태 유지
  - ALL_OFF/M_STOP 시 실행 context 정리
  - STOP으로 취소된 동일 transaction의 재전송 차단

## RS485 회로 검토 체크리스트

새 회로도를 검토할 때 다음 항목을 반드시 확인한다.

1. UART TX → DI
2. RO → UART RX
3. DE(pin 3) + `/RE`(pin 2) → DE_485
4. VCC(pin 8) → 3.3V
5. GND(pin 5) → GND
6. DE_485 GPIO 초기값 LOW
7. 송신 직전에만 DE_485 HIGH
8. UART TC 이후 DE_485 LOW 복귀
9. A/B 종단저항의 실장 위치
10. bias 제공 위치
11. reference GND 필요 여부

## 추가 작업 안전 원칙

- 기존 정상 기능을 삭제하지 않는다.
- warning을 숨기기 위한 목적으로 기능을 삭제하지 않는다.
- CubeMX 생성 파일을 수정해야 할 경우 USER CODE 영역을 우선 사용한다.
- 실제 장비 검증 전에는 정상 동작으로 단정하지 않는다.
