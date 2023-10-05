#include "SerialCOM.h"
#include <windows.h>
#include <stdio.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm")

// COM 포트 번호를 PC에 연결된 COM 포트 번호로 변경하십시오.
// iAHRS 센서의 기본 통신 속도는 115200 bps 입니다.
CSerialCOM com("\\\\.\\COM3", 115200);

int SendRecv(const char* command, double* returned_data, int data_length);

int main(void)
{
	const int TARGET_RESOLUTION	 = 1;	// 1-millisecond target resolution
	
	// Windows timer event가 1ms 주기로 동작하도록 합니다.
	TIMECAPS tc;
	timeGetDevCaps(&tc, sizeof(TIMECAPS));
	UINT timerRes = min(max(tc.wPeriodMin, TARGET_RESOLUTION), tc.wPeriodMax);
	timeBeginPeriod(timerRes); 

	com.Open();
	com.SetTimeout (1, 10, 0);

	if (com.IsConnected()) {
		double data1[10];
		double data2[10];
		double data3[10];

		for (int i = 0; i < 1000; i++) {
			DWORD t1 = timeGetTime(); 
			int no_data1 = SendRecv("e\n", data1, 10);	// Read Euler angle
			int no_data2 = SendRecv("a\n", data2, 10);	// Read Acceleration
			int no_data3 = SendRecv("t\n", data3, 10);
			DWORD t2 = timeGetTime(); 
			printf("[%d] Euler angle [deg] = %f, %f, %f\n", t2 - t1, data1[0], data1[1], data1[2]);
			printf("[%d] Acceleration [g] = %f, %f, %f\n", t2 - t1, data2[0], data2[1], data2[2]);
			printf("[%d] Temperature [C] = %f, %f\n", t2 - t1, data3[0]);
			Sleep(33);
		}
	}

	timeEndPeriod (timerRes);
}

int SendRecv(const char* command, double* returned_data, int data_length)
{
	#define COMM_RECV_TIMEOUT	100	// ms

	int command_len = strlen(command);

	com.Purge();
	com.Send(command, command_len);

	const int buff_size = 1024;
	int  recv_len = 0;
	char recv_buff[buff_size + 1];	// 마지막 EOS를 추가하기 위해 + 1

	DWORD time_start = GetTickCount();

	while (recv_len < buff_size) {
		int n = com.Recv(recv_buff + recv_len, buff_size - recv_len);
		if (n < 0) {
			// 통신 도중 오류 발생
			return -1;
		}
		else if (n == 0) {
			// 아무런 데이터도 받지 못했다. 1ms 기다려본다.
			Sleep(0);
		}
		else if (n > 0) {
			recv_len += n;

			// 수신 문자열 끝에 \r, \n이 들어왔는지 체크
			if (recv_buff[recv_len - 2] == '\r' && recv_buff[recv_len - 1] == '\n') {
				break;
			}
		}

		DWORD time_current = GetTickCount();
		DWORD time_delta = time_current - time_start;

		if (time_delta >= COMM_RECV_TIMEOUT) break;
	}
	recv_buff[recv_len] = '\0';

	// 에러가 리턴 되었는지 체크한다.
	if (recv_len > 0) {
		if (recv_buff[0] == '!') {
			return -1;
		}
	}

	// 보낸 명령과 돌아온 변수명이 같은지 비교한다.
	if (strncmp(command, recv_buff, command_len - 1) == 0) {
		if (recv_buff[command_len - 1] == '=') {
			int data_count = 0;

			char* p = &recv_buff[command_len];
			char* pp = NULL;

			for (int i = 0; i < data_length; i++) {
				if (p[0] == '0' && p[1] == 'x') {	// 16 진수
					returned_data[i] = strtol(p+2, &pp, 16);
					data_count++;
				}
				else {
					returned_data[i] = strtod(p, &pp);
					data_count++;
				}

				if (*pp == ',') {
					p = pp + 1;
				}
				else {
					break;
				}
			}
			return data_count;
		}
	}
	return 0;
}
