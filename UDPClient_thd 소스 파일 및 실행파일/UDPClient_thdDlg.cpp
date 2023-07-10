
// UDPClient_thdDlg.cpp : ���� ����
//

#include "stdafx.h"
#include "UDPClient_thd.h"
#include "UDPClient_thdDlg.h"
#include "afxdialogex.h"
#include "DataSocket.h"

#define SERVER_PORT_NUMBER 8000
#define IP_ADDR "127.0.0.1"
#define CL_PORT_NUMBER 8100
#define SEGMENTATION 8

bool Timeout = FALSE;
bool Timer_Run = FALSE;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CCriticalSection tx_cs;
CCriticalSection rx_cs;

// ���� ���α׷� ������ ���Ǵ� CAboutDlg ��ȭ �����Դϴ�.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// ��ȭ ���� �������Դϴ�.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �����Դϴ�.

														// �����Դϴ�.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CUDPClient_thdDlg ��ȭ ����



CUDPClient_thdDlg::CUDPClient_thdDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_UDPCLIENT_THD_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CUDPClient_thdDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_IPADDRESS1, m_ipaddr);
	DDX_Control(pDX, IDC_EDIT1, m_tx_edit_short);
	DDX_Control(pDX, IDC_EDIT2, m_tx_edit);
	DDX_Control(pDX, IDC_EDIT4, m_rx_edit);
}

BEGIN_MESSAGE_MAP(CUDPClient_thdDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_EN_CHANGE(IDC_EDIT1, &CUDPClient_thdDlg::OnEnChangeEdit1)
	ON_BN_CLICKED(IDC_SEND, &CUDPClient_thdDlg::OnBnClickedSend)
	ON_BN_CLICKED(IDC_CLOSE, &CUDPClient_thdDlg::OnBnClickedClose)
END_MESSAGE_MAP()


// CUDPClient_thdDlg �޽��� ó����
bool compare_checksum(Frame frm)
{
	unsigned short sum = 0; // ��� ���ذ� ����
	unsigned short checksum = 0;

	for (int i = 0; frm.buffer[i]; i++) {
		sum += frm.buffer[i];
	}

	checksum = (sum + frm.ms_length) + frm.checksum;
	if (checksum > 0x00010000)// ĳ���� �߻��� ���
	{
		checksum = (checksum >> 16) + (checksum & 0x0000FFFF); //ĳ�� ������
	}
	if ((0xFFFF - checksum) == 0)
		return TRUE;	//�������� X
	else
		return FALSE;	//��������
}

UINT TimerThread(LPVOID arg)
{
	ThreadArg *pArg = (ThreadArg*)arg;

	while (pArg->Thread_run) // �����尡 ���ư���
	{
		if (Timer_Run == TRUE) { // Ÿ�̸�����
			int SendOut = GetTickCount(); // Tick���� �޾ƿ�

			while (1) {
				if ((Timer_Run == FALSE)) {//�ð����� Ȯ���� �Ǹ� �����ۿ䱸x Ÿ�̸�����.
					break;
				}
				if ((GetTickCount() - SendOut) / CLOCKS_PER_SEC > 10)//�ִ� ��� �ð� 10��
				{//�ð��ʰ��� ������ ��û
					Timeout = TRUE;
					break;
				}
			}
		}
		Sleep(100);
	}
	return 0;
}

UINT RXThread(LPVOID arg) // ���� message ���
{
	ThreadArg *pArg = (ThreadArg *)arg; // Thread ����ü�����ͺ��� ����
	CList <Frame> *plist = pArg->pList; // Frame�� ����ü �迭 ����
	CUDPClient_thdDlg *pDlg = (CUDPClient_thdDlg*)pArg->pDlg; // �������� ��ü ��ġ ����

	Frame NFrame; // Frame ������ ������ plist�� ���� ���Ҹ� �����ϱ� ���� ����
	CString Mid[16]; // reassembly�� ���� ����
	CString Fullms;


	while (pArg->Thread_run)
	{
		POSITION pos = plist->GetHeadPosition();// ���� ���� �޽��� -> Head�� ����
		POSITION current_pos;

		while (pos != NULL) // message�� �����Ѵٸ�
		{
			current_pos = pos;

			rx_cs.Lock();
			NFrame = plist->GetNext(pos);//message frame ���ۿ� �ֱ�
			rx_cs.Unlock();


			if (compare_checksum(NFrame) == 0) // checksum�κп��� ������ �ִٸ� 
			{
				AfxMessageBox(_T("checksum error!"));
			}
			else
			{
				CString sq_num;
				sq_num.Format(_T("%d"), NFrame.seq_num); // sequence number �޾ƿ�
				if (NFrame.check == 0) // �������� �ƴҶ�
				{
					AfxMessageBox(sq_num + _T("������ ����!"));
					NFrame.ack_num = TRUE;
					pDlg->m_pDataSocket->SendToEx(&NFrame, sizeof(Frame), pArg->Dst_Port, pArg->Dst_Addr); // �������� ack ����
				}
				else
				{
					AfxMessageBox(sq_num + _T("�� �������� ������ ��"));
				}
			}

			//reassembly
			int ms_l = NFrame.ms_length;
			if ((NFrame.SourcePort == pArg->Dst_Port))
			{
				if (NFrame.check == 0) // ���������� �ʾƵ� �Ǵ� ���
				{
					if (NFrame.seq_num == ms_l / SEGMENTATION) {// seq_num�� ������ ��ȣ�� �����Ӱ� ��ġ�Ѱ��
						Mid[NFrame.seq_num] = (LPCTSTR)NFrame.buffer;
						// �غ��ص� Mid�迭�� sequence ��ȣ index�� �����ӿ� �ִ� ������ ���� �����
						for (int i = 0; i <= ms_l / SEGMENTATION; i++) // ������ ������ŭ
						{
							Fullms += Mid[i]; // ��ü ���ڿ��� Mid�迭�� ���ڵ� �߰�
							Mid[i] = _T("");
						}
					}
					else // �ƴϸ� ������ �����ӿ� �ִ� ���ڵ� �׳� �����
					{
						Mid[NFrame.seq_num] = (LPCTSTR)NFrame.buffer;
						plist->RemoveAt(current_pos);
						continue;
					}
				}
				else
				{
					Mid[NFrame.seq_num] = (LPCTSTR)NFrame.buffer;
					plist->RemoveAt(current_pos);
					continue;
				}
			}

			int len = pDlg->m_rx_edit.GetWindowTextLengthW();//ȭ����ü �ؽ�Ʈ���� �ҷ���
			pDlg->m_rx_edit.SetSel(len, len);
			pDlg->m_rx_edit.ReplaceSel(_T(" ") + Fullms);//���� �޾ƿ� �޼����� rxȭ�鿡 ���
			Fullms = _T("");//������ ������ �� �� =  ������ �޼��� ��Ƴ��� ���� �ʱ�ȭ
			plist->RemoveAt(current_pos);//���Ű����� �Ϸ�-> ���� ��ġ�� �ʱ�ȭ

		}
		Sleep(10);
	}
	return 0;
}
UINT TXThread(LPVOID arg)// message ����
{
	ThreadArg *pArg = (ThreadArg*)arg;
	CList<Frame> *plist = pArg->pList; //plist
	CUDPClient_thdDlg *pDlg = (CUDPClient_thdDlg*)pArg->pDlg; //��ȭ���� ����Ű�� ���� ����

	Frame NFrame;// Frame ������ ������ plist�� ���� ���Ҹ� �����ϱ� ���� ����

	while (pArg->Thread_run)
	{
		POSITION pos = plist->GetHeadPosition(); //pList�� ����� ��ġ ����
		POSITION current_pos;
		while (pos != NULL)
		{
			current_pos = pos; //list�� �� �պκ��� current_pos�� ����
			tx_cs.Lock(); //critical section
			NFrame = plist->GetNext(pos); //list�� �Ǿպκ� ����Ű���� ��ġ����
			tx_cs.Unlock();	//critical section����

			pDlg->m_pDataSocket->SendToEx(&NFrame, sizeof(Frame), SERVER_PORT_NUMBER, _T(IP_ADDR)); // ������ Frame ����

			Timer_Run = TRUE;
			while (1) {
				if (Timeout == TRUE) { // �ð��� �ʰ��Ǹ�
					NFrame.check = 1; // check�� 1 -> ������ ��û
					pDlg->m_pDataSocket->SendToEx(&NFrame, sizeof(Frame), SERVER_PORT_NUMBER, _T(IP_ADDR)); // ������
					Timeout = FALSE;

					int len = pDlg->m_tx_edit.GetWindowTextLengthW();
					CString message = _T("(TimeOut! retransmit the Frame...)\r\n"); // �ð��� �ٵǾ��ٰ� ǥ��
					pDlg->m_tx_edit.SetSel(len, len);
					pDlg->m_tx_edit.ReplaceSel(message);

					Sleep(10);
					break;
				}
				if (Timer_Run == FALSE) { // Ÿ�̸Ӱ� �ٵǸ� while�� ��������
					break;
				}
			}

			plist->RemoveAt(current_pos); //���� �� ���� �޼��� ����
		}
		Sleep(10);
	}
	return 0;
}

BOOL CUDPClient_thdDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// �ý��� �޴��� "����..." �޴� �׸��� �߰��մϴ�.

	// IDM_ABOUTBOX�� �ý��� ��� ������ �־�� �մϴ�.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// �� ��ȭ ������ �������� �����մϴ�.  ���� ���α׷��� �� â�� ��ȭ ���ڰ� �ƴ� ��쿡��
	//  �����ӿ�ũ�� �� �۾��� �ڵ����� �����մϴ�.
	SetIcon(m_hIcon, TRUE);			// ū �������� �����մϴ�.
	SetIcon(m_hIcon, FALSE);		// ���� �������� �����մϴ�.

									// TODO: ���⿡ �߰� �ʱ�ȭ �۾��� �߰��մϴ�.
	m_pDataSocket = NULL; // �ʱ�ȭ
	m_ipaddr.SetWindowTextW(_T(IP_ADDR));

	CList <Frame>* newlist = new CList <Frame>;
	arg1.pList = newlist;
	arg1.Thread_run = 1;
	arg1.pDlg = this;

	CList <Frame>* newlist2 = new CList <Frame>;
	arg2.pList = newlist2;
	arg2.Thread_run = 1;
	arg2.pDlg = this;
	arg2.r_seq_num = -1;

	CList<Frame> * newlist3 = new CList<Frame>;
	arg3.pList = newlist3;
	arg3.Thread_run = 1;
	arg3.pDlg = this;
	arg3.r_seq_num = -1;

	m_ipaddr.SetWindowTextW(_T("127.0.0.1"));

	WSADATA wsa;
	int error_code;

	if ((error_code = WSAStartup(MAKEWORD(2, 2), &wsa)) != 0)
	{
		TCHAR buffer[256];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 256, NULL);
		AfxMessageBox(buffer, MB_ICONERROR);
	} // ���� ó��

	m_pDataSocket = new CDataSocket(this);		//���� �Ҵ�
	m_pDataSocket->Create(CL_PORT_NUMBER, SOCK_DGRAM);		//���� ���� UDP �̹Ƿ� client�� ����
	m_pDataSocket->GetSockName(arg1.Dst_Addr, arg1.Dst_Port);

	AfxMessageBox(_T("Client�� �����մϴ�."), MB_ICONINFORMATION);
	pThread1 = AfxBeginThread(TXThread, (LPVOID)&arg1);
	pThread2 = AfxBeginThread(RXThread, (LPVOID)&arg2);
	pThread3 = AfxBeginThread(TimerThread, (LPVOID)&arg3);
	return TRUE;  // ��Ŀ���� ��Ʈ�ѿ� �������� ������ TRUE�� ��ȯ�մϴ�.
}

void CUDPClient_thdDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// ��ȭ ���ڿ� �ּ�ȭ ���߸� �߰��� ��� �������� �׸�����
//  �Ʒ� �ڵ尡 �ʿ��մϴ�.  ����/�� ���� ����ϴ� MFC ���� ���α׷��� ��쿡��
//  �����ӿ�ũ���� �� �۾��� �ڵ����� �����մϴ�.

void CUDPClient_thdDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // �׸��⸦ ���� ����̽� ���ؽ�Ʈ�Դϴ�.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Ŭ���̾�Ʈ �簢������ �������� ����� ����ϴ�.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// �������� �׸��ϴ�.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// ����ڰ� �ּ�ȭ�� â�� ���� ���ȿ� Ŀ���� ǥ�õǵ��� �ý��ۿ���
//  �� �Լ��� ȣ���մϴ�.
HCURSOR CUDPClient_thdDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CUDPClient_thdDlg::OnEnChangeEdit1()
{
	// TODO:  RICHEDIT ��Ʈ���� ���, �� ��Ʈ����
	// CDialogEx::OnInitDialog() �Լ��� ������ 
	//�ϰ� ����ũ�� OR �����Ͽ� ������ ENM_CHANGE �÷��׸� �����Ͽ� CRichEditCtrl().SetEventMask()�� ȣ������ ������
	// �� �˸� �޽����� ������ �ʽ��ϴ�.

	// TODO:  ���⿡ ��Ʈ�� �˸� ó���� �ڵ带 �߰��մϴ�.
}
void CUDPClient_thdDlg::OnBnClickedSend()
{
	// TODO: ���⿡ ��Ʈ�� �˸� ó���� �ڵ带 �߰��մϴ�.
	CString tx_message;
	Frame frm;
	frm.SourcePort = arg1.Dst_Port;
	m_tx_edit_short.GetWindowText(tx_message); // message �޾ƿ���


	frm.ms_length = tx_message.GetLength();

	if ((frm.ms_length / SEGMENTATION) == 0) // �������� ���
	{
		tx_message += _T("\r\n"); // �ٹٲ� ���ڸ� ������
		_tcscpy_s(frm.buffer, (LPCTSTR)tx_message);

		frm.seq_num = 0;
		frm.checksum = checksum_result(frm);
		arg1.r_seq_num = 0;

		tx_cs.Lock();//�Ӱ豸��
		arg1.pList->AddTail(frm); // arg1 ������ ���� ������ ������ ������ ������ �ٿ���
		arg3.pList->AddTail(frm); // timer �����忡 ���� ����
		tx_cs.Unlock();//�Ӱ豸��
	}
	else // �׷��� ���� ���
	{
		int i = 0;
		for (i = 0; i < (frm.ms_length / SEGMENTATION); i++) // �ʿ��� ������ ��ŭ
		{
			_tcscpy_s(frm.buffer, (LPCTSTR)tx_message.Mid(i * SEGMENTATION, SEGMENTATION));
			frm.seq_num = i; // �� ������ ���� ������ �ѹ��� ��������
			frm.checksum = checksum_result(frm); // ������ ���� Checksum ������ ����

			tx_cs.Lock();//�Ӱ豸��
			arg1.pList->AddTail(frm); // arg1 ������ ���� ������ ������ ������ ������ �ٿ���
			arg3.pList->AddTail(frm);// timer �����忡 ���� ����
			tx_cs.Unlock();//�Ӱ豸��
		}
		tx_message += _T("\r\n");//�ٹٲ� ���ڸ� �߰�����
		CString EndOfString = tx_message.Mid(i * SEGMENTATION, (frm.ms_length - i*SEGMENTATION) + 3); // i*SEGMENTATION���� ���� frm���� - i*SEGMENTATION + 3���� ���� ����
		_tcscpy_s(frm.buffer, EndOfString); // �����ӿ� �߰� �� �Ŀ� ���� �ٳ��� ���� �����ӵ� �߰�����
		frm.seq_num = i; // ������ ��ȣ i
		frm.checksum = checksum_result(frm); // üũ�� ����
		arg1.r_seq_num = i; // �ֱ� ������ number�� i�� ����

		tx_cs.Lock();//�Ӱ豸�� ���
		arg1.pList->AddTail(frm);// arg1 ������ ���� ������ ������ ������ ������ �ٿ���
		arg3.pList->AddTail(frm);// timer �����忡 ���� ����
		tx_cs.Unlock();//�Ӱ豸�� ����

	}//SEGMENTATION
	m_tx_edit_short.SetWindowText(_T(""));// �ʱ�ȭ
	m_tx_edit_short.SetFocus();

	int len = m_tx_edit.GetWindowTextLengthW();//ȭ����ü �ؽ�Ʈ���� �ҷ���
	m_tx_edit.SetSel(len, len);//Ŀ���� ���ڿ� �������� ��ġ
	m_tx_edit.ReplaceSel(_T("") + tx_message);//���� ������ �޼����� ����â�� ���
}


void CUDPClient_thdDlg::OnBnClickedClose() // ä��â close
{
	if (m_pDataSocket == NULL) {
		MessageBox(_T("������ ���� �� ��!"), _T("�˸�"), MB_ICONERROR);
	}
	else
	{
		arg1.Thread_run = 0;
		arg2.Thread_run = 0;
		m_pDataSocket->Close();
		delete m_pDataSocket;
		m_pDataSocket = NULL;
	}
}



unsigned short CUDPClient_thdDlg::checksum_result(Frame send_frame)
{
	unsigned short sum = 0; // ��� ���ذ� ����
	unsigned short checksum = 0;

	for (int i = 0; send_frame.buffer[i]; i++) {
		sum += send_frame.buffer[i]; // ���ۿ� ��� ���� �� ������
	}

	checksum = sum + send_frame.ms_length;
	if (checksum > 0x00010000)// ĳ���� �߻��� ���
	{
		checksum = (checksum >> 16) + (checksum & 0x0000FFFF); //ĳ�� ������
	}

	checksum = ~(unsigned short)(checksum);
	return checksum;
}

void CUDPClient_thdDlg::ProcessClose(CDataSocket* pSocket, int nErrorCode)
{
	arg1.Thread_run = 0;
	arg2.Thread_run = 0;
	pSocket->Close();
	delete m_pDataSocket;
	m_pDataSocket = NULL;

	int len = m_rx_edit.GetWindowTextLengthW();
	CString tx_Message = _T("##��������##\r\n");
	m_rx_edit.SetSel(len, len);
	m_rx_edit.ReplaceSel(tx_Message);
}


void CUDPClient_thdDlg::ProcessReceive(CDataSocket* pSocket, int nErrorCode)
{
	Frame frm;
	UINT PeerPort;
	CString PeerAddr;

	if (pSocket->ReceiveFromEx(&frm, (sizeof(Frame)), PeerAddr, PeerPort) < 0)//UDP ������ �����ߴµ� ������ ������
	{
		AfxMessageBox(_T("Receive Error"), MB_ICONERROR);
	}
	if (frm.ack_num == FALSE) { //ack�� �ȿ�����
		rx_cs.Lock();
		arg2.pList->AddTail(frm);//CStringŸ���� ���� �����͸� rx����Ʈ�� �߰���
		arg2.Dst_Addr = (LPCTSTR)PeerAddr; // �޾ƿ� �ּ�
		arg2.Dst_Port = PeerPort; // �޾ƿ� port��ȣ rx����Ʈ�� �߰�
		rx_cs.Unlock();
	}
	else if ((frm.ack_num == TRUE)) { // ack�� ������
		Timer_Run = FALSE; // Ÿ�̸� ����
	}
}

