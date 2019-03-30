#include "stdafx.h"
#include "windows.h"
#include <winsock2.h>
#pragma comment (lib, "Ws2_32.lib")
#include "b64.h"

char *dname[] = { "Mon", "Tue", "Wed" , "Thu", "Fri", "Sat", "Sun" };

char *mname[] = { "Jan" , "Feb" , "Mar" , "Apr" , "May" , "June" ,
				  "July" , "Aug" , "Sept" , "Oct" , "Nov" , "Dec" };

int __cdecl DbpErr(LPCTSTR pFormat, ...)
{
	static TCHAR strBuf[1024];
	PVOID lpMsgBuf;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR)&lpMsgBuf,
		0,
		nullptr
	);

	va_list pArg;

	va_start(pArg, pFormat);
	wvsprintf(strBuf, pFormat, pArg);
	va_end(pArg);

	return MessageBoxEx(nullptr, (LPCTSTR)lpMsgBuf, (LPCTSTR)strBuf, MB_OK, 0001);
}

#define EQ(x,y) ( !lstrcmpi((x),(y)) )
#define lxsend(s,buf) send((s),(buf),lstrlen(buf),0)
#define lxrecv(s,buf) recv(s,buf,sizeof(buf),0)

inline int Errcode(const char buf[], const char code[])
{
	for (int i = 0; i < 3; i++)
	{
		if (buf[i] != code[i])
		{
			return 1;
		}
	}

	return 0;
}

int ErrEnd(SOCKET h, char buf[], char code[])
{
	char tmp[128];

	lxsend(h, buf);
	lxrecv(h, tmp);

	if (Errcode(tmp, code))
	{
		char err[256];
		wsprintf(err, "Error:%s\r\n\r\nMsg;%s", tmp, buf);
		DbpErr(err);
		//MsgPrint(err);
		ExitProcess(0);
	}

	return 0;
}

int makedata(char strmail[], char subject[], char data[], char user[], char domain[], char to[], char from[])
{
	char tmp[1024];
	wsprintf(strmail, "From: \"%s\" <%s>\r\n", user, from);

	wsprintf(tmp, "To: <%s>\r\n", to);
	lstrcat(strmail, tmp);

	wsprintf(tmp, "Subject: %s\r\n", subject);
	lstrcat(strmail, tmp);

	SYSTEMTIME	SysTime;
	GetLocalTime(&SysTime);

	wsprintf(tmp, "Date: %s,%d %s %d %2d:%2d:%2d +0800\r\n",
		dname[SysTime.wDayOfWeek],
		SysTime.wDay,
		mname[SysTime.wMonth],
		SysTime.wYear,
		SysTime.wHour,
		SysTime.wMinute,
		SysTime.wSecond
	); //"Date: Thu, 30 Aug 2007 15:23:32 +0800\r\n";  //Ô¤ÁôÈÕÆÚ

	lstrcat(strmail, tmp);
	wsprintf(tmp, "Content-Type: multipart/mixed; boundary=\"#BOUNDARY.CMAILSERVER#\"\r\n\r\n\r\n");
	lstrcat(strmail, tmp);
	wsprintf(tmp, "--#BOUNDARY.CMAILSERVER#\r\n");
	lstrcat(strmail, tmp);
	wsprintf(tmp, "Content-Type: text/html; charset=\"gb2312\"\r\n"
		"Content-Transfer-Encoding: 7bit\r\n\r\n");
	lstrcat(strmail, tmp);

	lstrcat(strmail, data);

	wsprintf(tmp, "\r\n\r\n--#BOUNDARY.CMAILSERVER#--\r\n\r\n.\r\n");
	lstrcat(strmail, tmp);

	return 0;
}

int SendMail(char from[], char to[], char domain[], char user[], char pass[], char subject[], char data[])
{
	WSADATA wsaData;
	const WORD ver = MAKEWORD(2, 2);
	WSAStartup(ver, &wsaData);

	SOCKET h = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);

	struct hostent *ser = gethostbyname((LPSTR)domain);

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(25);
	addr.sin_addr.S_un.S_addr = inet_addr(inet_ntoa(*((struct in_addr *)ser->h_addr)));

	const auto rt = connect(h, (SOCKADDR*)&addr, sizeof(addr));

	if (rt == SOCKET_ERROR)
	{
		DbpErr("Connect");
		return 0;
	}

	char buf[1024 * 512], tmp[512];

	lxrecv(h, buf);	//return msg

	wsprintf(buf, "HELO %s\r\n", "localhost");		//HELO

	lxsend(h, buf);
	lxrecv(h, buf);

	wsprintf(buf, "AUTH LOGIN\r\n");		//login

	ErrEnd(h, buf, "334");

	Base64Encode(tmp, user, lstrlen(user));	//user

	wsprintf(buf, "%s\r\n", tmp);

	ErrEnd(h, buf, "334");

	Base64Encode(tmp, pass, lstrlen(pass));	//pass
	wsprintf(buf, "%s\r\n", tmp);

	ErrEnd(h, buf, "235");	//Authentication successful

	wsprintf(buf, "MAIL FROM:<%s>\r\n", from);	//from

	ErrEnd(h, buf, "250");

	wsprintf(buf, "RCPT TO:<%s>\r\n", to);	//to

	ErrEnd(h, buf, "250");

	wsprintf(buf, "DATA\r\n");	//start data

	lxsend(h, buf);
	lxrecv(h, buf);

	makedata(buf, subject, data, user, domain, to, from);	//datas

	ErrEnd(h, buf, "250");

	wsprintf(buf, "\r\n.\r\n");	//end mail

	lxsend(h, buf);
	lxrecv(h, buf);

	wsprintf(buf, "QUIT\r\n");	//QUIT

	lxsend(h, buf);
	lxrecv(h, buf);

	WSACleanup();

	return 0;
}

int main(int argc, char* argv[])
{
	char from[128] = "mailhost@126.com", to[128] = "mailhost@126.com", subject[128] = "test send";
	char user[64] = "mailhost", pass[64] = "", domain[] = "smtp.126.com";

	char *data = argv[1];

	SendMail(from, to, domain, user, pass, subject, data);

	return 0;
}
