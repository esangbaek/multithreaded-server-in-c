# Project - 1 : Multithreaded TCP Server

### What is this program?

기존 single-threaded version은 한번에 하나의 connection만 다룰수 있었는데,

multi-threaded 구조로 구현을 하면 동시에 여러개의 연결을 처리할 수 있다.



- N개의 threads들은 Queue에 등록된 수 들이 소수인지 연산한다.
- M개의 acceptor threads들은 client로 부터 받은 입력을 Queue에 추가한다.
- 1개의 signal thread는 idle한 thread를 깨워 Queue 값을 처리하도록 한다.

&nbsp;

### Input and Output

클라이언트는 다음과 같이 입력을 하여야 한다.

> [입력할 수의 개수] 숫자 숫자 숫자 ....
>
> ex) 10 1 2 3 4 5 6 7 8 9 10

서버에서는 " "를 기준으로 숫자를 parsing 한다.



### Goal

Thread pool의 사이즈와 Acceptor threads의 개수를 변경하면서 처리성능이 어떻게 달라지는지 확인해본다.

Thread pool이 커지면 Queue에서 한번에 처리할 수 있는 값의 개수가 늘어나 성능이 향상될 것 이며,

Acceptor threads 개수가 많아지면 Queue에 동시에 등록하는 threads가 증가하므로 thread pool을 효과적으로 사용할 것이다.



### How to build executable file

```bash
~$ ls
proj1

~$ cd proj1/
~/proj1$ make

server run example
~/proj1$ ./multisrv -n 8 -a 2
server using port 53233

client run example
~/proj1$ SERVERHOST=localhost ./echocli 53233
```

- option
  - -n N: Thread pool size	(default = 4)
  - -a M: Acceptor threads size   (default = 1)



