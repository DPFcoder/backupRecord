/** $Id: //depot/NVS/v2.1/bsrDirectory/common/SQL.hpp#4 $ $DateTime: 2008/12/05 16:22:45 $
*  @file SQL.hpp
*  @brief SQL header file
*  @version 1.0.5
*  @since 0.0.1
*  @author qiaowj<qiaowj@bstar.com.cn> 
*  @date 2006-06-10    Create it
*/
/************************************************************
*  @note
Copyright 2005, BeiJing Bluestar Corporation, Limited	
ALL RIGHTS RESERVED			
Permission is hereby granted to licensees of BeiJing Bluestar, Inc.	 products to use or abstract this computer program for the sole	 purpose of implementing a product based on BeiJing Bluestar, Inc. products. No other rights to reproduce, use, or disseminate this computer program,whether in part or in whole, are granted. BeiJing Bluestar, Inc. makes no representation or warranties with respect to the performance of this computer program, and specifically disclaims any responsibility for any damages, special or consequential, connected with the use of this program.
For details, see http://www.bstar.com.cn/ 
***********************************************************/

#ifndef __SQL_HPP
#define __SQL_HPP
#define ERR_MYSQL 200

#include <mysql++.h>
#include <iostream>
#include <string>
#include <map>
#include <stdlib.h>
#include <math.h>
//#include <syslog.h>

#include "Util.hpp"
#include "XmlRpc.h"


using namespace std;
using namespace mysqlpp;
using namespace XmlRpc;

enum DockType
{
  DOCK_TYPE_DVR = 1,
  DOCK_TYPE_NVR = 2,
  DOCK_TYPE_ENCODER = 3
};

///< 为了mysql中定义的类型与xmlrpc返回的类型一致，对mysql类型进行了以下分配
///< 注意：由于mysql中timestamp类型被定义为INT UNSIGNED NOT NULL，但查询结果又是一字符串，
///< 所以在bsrDirectory中INT UNSIGNED NOT NULL仅表示timestamp类型，定义整型时应避免使用该类型
#define mysqlInt(type) \
	type=="INT NOT NULL"||type=="INT NULL"||type=="INT UNSIGNED NULL"||type=="TINYINT NOT NULL"||type=="TINYINT NULL"||type=="BIGINT NOT NULL"||type=="BIGINT NULL"

#define mysqlChar(type) \
	type=="VARCHAR NOT NULL"||type=="VARCHAR NULL"||type=="CHAR NOT NULL"||type=="CHAR NULL"||type=="BLOB NOT NULL"||type=="BLOB NULL"

#define mysqlDatetime(type) \
	type=="VARCHAR NOT NULL"||type=="VARCHAR NULL"||type=="CHAR NOT NULL"||type=="CHAR NULL"||type=="BLOB NOT NULL"||type=="BLOB NULL"

#define mysqlTimestamp(type) \
	type=="INT UNSIGNED NOT NULL"

#define mysqlFloatDouble(type) \
	type=="FLOAT NOT NULL"||type=="FLOAT NULL" || type=="DOUBLE NOT NULL"||type=="DOUBLE NULL"

// other type not use "TIME NOT NULL" , "DATE NOT NULL", "DATETIME NOT NULL"

/// Xmlrpc ValueStruct reference
typedef std::map<std::string, XmlRpcValue> ValueStruct;
struct myKey
{
  string key;
  string value;
  string operate;
};

namespace global
{
	/// page info
  const int defaultPageNumber = 1;
  const int defaultPageRows = 50;
  const int defaultMaxPageNumber = 65535;
  const int defaultMaxPageRows = 65535;
};

//析构锁
class AutoMutex
{
public:
	AutoMutex(pthread_mutex_t &_mutex) {mutex = &_mutex; pthread_mutex_lock(mutex);}
	~AutoMutex() {pthread_mutex_unlock(mutex);}
private:
	pthread_mutex_t *mutex;
};

/** @class BsrSQL
* @brief 目录服务器mysql控制类
*/
class BsrSQL
{
protected:
  string m_sql;
  Result m_res;
  int m_affectedNum;
  vector<string>	m; //param list
  int m_errorCode;
  string m_errorStr;

private:
  mysqlpp::Connection*	m_con;
	pthread_mutex_t mutex;				//线程同步

public:
  BsrSQL(mysqlpp::Connection* _con = NULL);
  ~BsrSQL();

  void reset();
	int Ping() { return m_con->ping();}					//reconnect mysql,if recived respond return 0, else return nozero

  void setCon(mysqlpp::Connection* con){m_con = con;}
  mysqlpp::Connection* getCon() const { return m_con; };

  void setSql(const string& sql){m_sql = sql;}
  bool connectToDb(const ::MysqlParams& mp);
  void disConnect();

  ///PUBLIC Mysql API begin
  //mysql operator as string
  bool exec(XmlRpcValue& result);
  bool load(XmlRpcValue& result);

  //mysql operator as template
  void add(XmlRpcValue& params, XmlRpcValue& result, const string& table);
  void edit(XmlRpcValue& params, XmlRpcValue& result,const string& table);
  void del(XmlRpcValue& params, XmlRpcValue& result, const string& table);
  void list(XmlRpcValue& params, XmlRpcValue& result, const string& table, const string& sortstr = "id");
  void show(XmlRpcValue& params, XmlRpcValue& result, const string& table);
  //op database
  void mylist(XmlRpcValue& params, XmlRpcValue& result, const string& table);
  void myshow(XmlRpcValue& params, XmlRpcValue& result, const string& table);
  void myadd(XmlRpcValue& params, XmlRpcValue& result, const string& table);
  void myedit(XmlRpcValue& params, XmlRpcValue& result, const string& table);
  void mydel(XmlRpcValue& params, XmlRpcValue& result, const string& table);
  void multilist(XmlRpcValue& params, XmlRpcValue& result, const string& table, const string& sortstr = "id");
  void multiedit(XmlRpcValue& params, XmlRpcValue& result, const string& table);
  void multidel(XmlRpcValue& params, XmlRpcValue& result, const string& table);
  //store params's names to class member m
  bool getParamList(XmlRpcValue& params);
  //get one RpcValue'type, include TypeInt, TypeString
  XmlRpc::XmlRpcValue::Type getType(XmlRpcValue& param);
  //check params, return number of the params element, store params's names to class memeber m,set value for keyword
  int checkParams(XmlRpcValue& params, string& keyword, string& value);
  //get select condition
  int getCondition(XmlRpcValue& params, vector<myKey> &keyList);
  //check the node whether empty
  void checkNodeDel(XmlRpcValue& params,XmlRpcValue& result, const string& table, const string& key);
  ///PUBLIC Mysql API end

  Result store(const string &sqlStr, int *numRows);
  //成功执行返回true，否则返回false，主要用于增删改语句
	bool  execute(const string &sqlStr, int *numRows);
  /// special Mysql API for other service
  void show(int id, XmlRpcValue& result, string table);

  // add by chenwei<chenw@bstar.com.cn> in 08-01-16
  // Once will have to redo when has Lost connection to MySQL server during query
  bool query(const string& sql, mysqlpp::Result& res, int& nRows);
  bool static query(mysqlpp::Connection& con, string sql, mysqlpp::Result& res, int& nRows);

}; //BsrSQL end



#endif // !defined(MYSQLPP_UTIL_H)
