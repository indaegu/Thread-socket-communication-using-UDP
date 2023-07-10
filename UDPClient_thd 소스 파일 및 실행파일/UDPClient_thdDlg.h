
// UDPClient_thdDlg.h : ��� ����
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "DataSocket.h"

typedef struct
{

	TCHAR buffer[256]; // ������ �޼����� ��� �ֱ� ���� ����
	int seq_num = -1; // sequence number�� ������ ��ȣ�� ��������
	int ms_length = 0; // message�� ���̸� ����
	unsigned short checksum = 0; // üũ�� ���� ����
	bool ack_num = FALSE; // ack �ѹ��� �������� 0,1 �ΰ��� 2bit�� ���� ����
	UINT SourcePort;
	CString SourceAddress;
	int check = 0;// check�� 0�̸� pass, 1�̸� retransmit


}Frame;
struct ThreadArg
{
	CList <Frame>*pList;
	CDialogEx* pDlg;
	int Thread_run;
	int r_seq_num;
	UINT Dst_Port = -1;
	CString Dst_Addr;
};

class CDataSocket;

// CUDPClient_thdDlg ��ȭ ����
class CUDPClient_thdDlg : public CDialogEx
{
	// �����Դϴ�.
public:
	CUDPClient_thdDlg(CWnd* pParent = NULL);	// ǥ�� �������Դϴ�.

												// ��ȭ ���� �������Դϴ�.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_UDPCLIENT_THD_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV �����Դϴ�.


														// �����Դϴ�.
protected:
	HICON m_hIcon;

	// ������ �޽��� �� �Լ�
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnEnChangeEdit1();
	CWinThread*pThread1, *pThread2, *pThread3; // TX, RX, TIMER �����忡 ���� ����
	ThreadArg arg1, arg2, arg3;
	CDataSocket*m_pDataSocket;
	CIPAddressCtrl m_ipaddr;
	CEdit m_tx_edit_short; // ����â �� �Է�â
	CEdit m_tx_edit; // ����â
	CEdit m_rx_edit;// ����â
	afx_msg void OnBnClickedSend();
	afx_msg void OnBnClickedClose();
	unsigned short checksum_result(Frame);
	void ProcessClose(CDataSocket* pSocket, int nErrorCode);
	void ProcessReceive(CDataSocket* pSocket, int nErrorCode);
};
