#ifndef _H_COMMON_RPC_VERSION_H_
#define _H_COMMON_RPC_VERSION_H_
#define RPC_SERVER_VERSION "1.0.0"
#define RPC_SERVER_NAME    "RpcServer"
#define FULL_VERSION  RPC_SERVER_NAME " " RPC_SERVER_VERSION
const char FullVersion[] = FULL_VERSION;
static char CompileInfo[]=  __TIME__ " " __DATE__;
#ifdef EN_HELP
static char HelpMsg[]="\
==================================================================================================\n\
  Usage: " RPC_SERVER_NAME " [options][-c "RPC_SERVER_NAME".ini]\n\
  Options:\n\
  -p port     listen port.\n\
  -p port     Basic server name displayed at agent node.\n\
  -c config   configuration file "NAME".ini.\n\
  -d daemon   Running daemon.\n\
  -l loglevel log level comments:\n\
              1. ERROR: error.\n\
              2. WARN:  warning.\n\
              3. NOTICE:  notice.\n\
              4. INFO:  important information.\n\
              5. VARS:  variable info.\n\
              6. DEBUG: debug info, used by programmer.\n\
              7. ALL:   all info, lowest level.\n\
              default: 2\n\
  -v          Version informations.\n\
  -h          This help message\n\
  e.g.:  " RPC_SERVER_NAME " -p 9090 -c "RPC_SERVER_NAME".ini -l 2 -d\n\
==================================================================================================\r\n";
#else
static char HelpMsg[]="\
==================================================================================================\n\
  ʹ�÷���: " RPC_SERVER_NAME " [����ѡ��][-c �����ļ�]\n\
  ����ѡ������:\n\
  -p �����˿� ��������ʱ�󶨵ļ����˿ڡ�\n\
  -s �������� ��������ʱ�ṩ�ķ������ơ�\n\
  -c �����ļ���\n\
  -d ��̨����  �ػ����̷�ʽ���С�\n\
  -l ��־����  ��־���������������\n\
               1. ������־��\n\
               2. ������־��\n\
               3. ֪ͨ��־��\n\
               4. ��Ҫ����ʾ����־��\n\
               5. ��ӡ�ؼ�������ֵ��\n\
               6. ������Ա�ĵ�����־��\n\
               7. �����е���־��\n\
  -v           ����汾��Ϣ��\n\
  -h           ������а�����Ϣ��\n\
  ����ʵ����   " RPC_SERVER_NAME "  -p 9090 -c "RPC_SERVER_NAME".ini -l 2 -d\n\
==================================================================================================\r\n";
#endif // EN_HELP
#endif // _H_COMMON_RPC_VERSION_H_