#ifndef _H_AGENT_VERSION_H_
#define _H_AGENT_VERSION_H_
#define VERSION "1.0.1"
#define NAME    "agent"
#define AGENT_FULL_VERSION  NAME " " VERSION
const char FullVersion[] = AGENT_FULL_VERSION;
static char CompileInfo[]=  __TIME__ " " __DATE__;
#ifdef EN_HELP
static char HelpMsg[]="\
==============================================================================\n\
  Usage: " NAME " [options][-c "NAME".ini]\n\
  Options:\n\
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
  e.g.:  " NAME " -c "NAME".ini -l 2 -d\n\
==============================================================================\r\n";
#else // EN_HELP
static char HelpMsg[]="\
==============================================================================\n\
  ʹ�÷���: " NAME " [����ѡ��][-c �����ļ�]\n\
  ����ѡ������:\n\
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
  ����ʵ����   " NAME " -c "NAME".ini -l 2 -d\n\
==============================================================================\r\n";
#endif // EN_HELP
#endif // _H_AGENT_VERSION_H_
