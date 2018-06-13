/** $Id: //depot/NVS/v2.1/bsrDirectory/common/SQL.cpp#4 $ $DateTime: 2008/12/05 16:22:45 $
 *  @file SQL.cpp
 *  @brief SQL .cpp file
 *  @version 1.0.5
 *  @since 0.0.1
 *  @author qiaowj<qiaowj@bstar.com.cn>
 *  @date 2006-06-10    Create it
 */
 /************************************************************
 *  @note
 Copyright 2005, BeiJing Bluestar Corporation, Limited
 ALL RIGHTS RESERVED
 Permission is hereby granted to licensees of BeiJing Bluestar, Inc.   products to use or abstract this computer program for the sole   purpose of implementing a product based on BeiJing Bluestar, Inc. products. No other rights to reproduce, use, or disseminate this computer program,whether in part or in whole, are granted. BeiJing Bluestar, Inc. makes no representation or warranties with respect to the performance of this computer program, and specifically disclaims any responsibility for any damages, special or consequential, connected with the use of this program.
 For details, see http://www.bstar.com.cn/
 ***********************************************************/

#include "SQL.hpp"
 //#include "Common.hpp"
 //进程线程日志flag
#define THREAD 1
#define NOTHREAD 0

using namespace std;
using namespace mysqlpp;


BsrSQL::BsrSQL(mysqlpp::Connection* _con /* = NULL */)
{
  m_con = _con;
  m_sql = "";
  m_affectedNum = 0;
  //对象用NULL来初始化,有问题.
  m_res = NULL;
  m_errorCode = 0;
  m_errorStr = "";

  pthread_mutex_init(&mutex, NULL);
}

BsrSQL::~BsrSQL()
{
  if (m_res)
  {
    m_res.purge();
    m_res = NULL;
  }
  m_sql = "";
  m_affectedNum = 0;
  m.clear();
  m_errorCode = 0;
  m_errorStr = "";
}

void BsrSQL::reset()
{
  if (m_res)
  {
    m_res.purge();
    m_res = NULL;
  }
  m_sql = "";
  m_errorStr = "";
  m_affectedNum = 0;
  m.clear();
  m_errorCode = 0;
}


/**
* @fn BsrSQL::connectToDb(const MysqlParams& mp)
* @brief connect mysql database
* @param[in] con   an mysqlpp::Connection argument.
* @return    connect is or not success,
* @retval  true:  OK
* @retval  false:  fail
*/
bool BsrSQL::connectToDb(const ::MysqlParams& mp)
{
  // changed by weizhixiang in 2018-3-5 10:35
  m_con->set_option(mysqlpp::Connection::opt_reconnect, true);
  int i = 0;
redo:
  try {
	  if (!m_con->connect(mp.database, mp.serverAddr, mp.username, mp.password))
	  {
		  LOG_DEBUG(NOTHREAD, "Connect fail, DB:%s,Server:%s,User:%s,PWD:%s",
			  mp.database, mp.serverAddr, mp.username, mp.password);
		  return false;
	  }
	  return true;
  }

  catch (const mysqlpp::BadQuery& er) {
	  cerr << "Query error: \"" << er.what() << "\"" << endl;
	  string errstr = er.what();
	  if (errstr == "Lost connection to MySQL server during query" && ++i <= 3) {
		  sleep(1);
		  goto redo;
	  }
	  return false;
  }
  catch (const mysqlpp::Exception& er) {
	  // Catch-all for any other MySQL++ exceptions
	  cerr << "Error: " << er.what() << endl;
	  return false;
  }
}

void BsrSQL::disConnect()
{
  if (m_con != NULL)
  {
    m_con->close();
    delete m_con;
    m_con = NULL;
  }
}


Result BsrSQL::store(const string &sqlStr, int *numRows)
{
  AutoMutex mutexDestory(mutex);			//线程同步
  //效率问题
  Result res;

  Query query = m_con->query();
  query << sqlStr;
  res = query.store();
  *numRows = (int)(m_con->affected_rows());

  return res;
}



/*
bool readDBSysConf()
{
  mysqlpp::Query query = g_con.query();
  query << "SELECT name, value FROM System WHERE category='internal'";


  mysqlpp::Result ret = query.store();
  if(!ret)
  {

    g_cfg["recordPeriod"] = "5";
    g_cfg["decoderTitle"] = "1";
    g_cfg["decoderTitlePos"] = "1";
    g_cfg["checkEncoder"] = "1";
    g_cfg["checkEncIntvl"] = "10";
    g_cfg["systemSessionLimit"] = "500";
    g_cfg["sessionStreamLimit"] = "500";
    g_cfg["systemStreamOutLimit"] = "500";
    g_cfg["systemStreamInLimit"] = "500";
    g_cfg["userRegNotice"] = "MS";
    g_cfg["logLanguage"] = "english";

    return false;
  }

  mysqlpp::Row row;
  while(row = ret.fetch_row())
  {
    string name = string(row[(unsigned int)0]);
    string value = string(row[1]);
    g_cfg[name] = value;

  }
  return true;
}
*/

/**
* @fn getParamList
* @brief 检查params是否有数据库参数存在
将得到的值的first存入成员m中
* @param[in] params an XmlRpcValue argument.是客户端传来的参数
*/
bool BsrSQL::getParamList(XmlRpcValue& params)
{
  bool b = false;
  if (!params.valid())
    return b;

  ValueStruct* p = params.getValueStruct();
  ValueStruct::iterator it = p->begin();
  while (it != p->end())
  {
    if ((it->first) == "")
    {
      it++;
      continue;
    }
    if (((it->first).find_first_of("_")) == 0)
    {
      it++;
      continue;
    }

    m.push_back(it->first);
    b = true;
    it++;
  }
  return b;
}



/**
* @fn getType
* @brief get data-type of param
* @param[in] param  an XmlRpcValue argument.
* @return  return the define value by  XmlRpc::XmlRpcValue::Type
* @see the defination of the XmlRpc Class
*/
XmlRpc::XmlRpcValue::Type BsrSQL::getType(XmlRpcValue& param)
{
  return param.getType();
}


/**
* @fn checkParams
* @brief 获取客户端传过来的条件
* @param[in] params  传入的客户端参数.
* @param[out]  keyword   客户端传过来的关键字，
如果有参数bkeyword指定了关键字，则keyword为指定关键字
如果有参数bkeyword没有指定关键字，则"id"作为默认关键字
* @param[out]  value 存放keyword的值
* @return    返回params包含的参数的个数
* @retval  >0:  条件个数
* @retval  <0:  参数错误
* @retval  =0:   没有条件
*/


int BsrSQL::getCondition(XmlRpcValue& params, vector<myKey> &keyList)
{
  if (params.valid())
  {
    if (getType(params) != XmlRpc::XmlRpcValue::TypeStruct)
    {
      m_errorCode = ERR_MYSQL + 7;
      m_errorStr = "Param type must be struct for getCondition(), correct eg: param[\"_cond\"][0][\"key\"]";
      return -1;
    }
  }
  else
  {
    return 0;
  }

  if (params.hasMember("_cond"))
  {
    if (getType(params["_cond"]) != XmlRpc::XmlRpcValue::TypeArray)
    {
      m_errorCode = ERR_MYSQL + 8;
      m_errorStr = "Param _cond must be array for getCondition(), correct eg: param[\"_cond\"][0][\"key\"]";
      return -2;
    }
  }
  else
  {
    return 0;
  }

  XmlRpcValue arrayValue = params["_cond"];
  int condSize = arrayValue.size();
  for (int i = 0; i < condSize; ++i)
  {
    if (!arrayValue[i].valid())
    {
      m_errorCode = ERR_MYSQL + 9;
      m_errorStr = "array invalid, array subscript must is sequence for getCondition(), correct eg: param[\"_cond\"][0][\"key\"]";
      return -3;
    }
    XmlRpcValue structValue = arrayValue[i];

    if (getType(structValue) != XmlRpc::XmlRpcValue::TypeStruct)
    {
      m_errorCode = ERR_MYSQL + 10;
      m_errorStr = "struct invalid, struct subscript must is a string for getCondition(), correct eg: param[\"_cond\"][0][\"key\"]";
      return -4;
    }

    if (!structValue["key"].valid())
    {
      m_errorCode = ERR_MYSQL + 11;
      m_errorStr = "key is null for getCondition(), correct eg: param[\"_cond\"][0][\"key\"]";
      return -5;
    }

    if (!structValue["value"].valid())
    {
      m_errorCode = ERR_MYSQL + 12;
      m_errorStr = "key is exist, but can't find match value for getCondition(), correct eg: param[\"_cond\"][0][\"key\"], and param[\"_cond\"][0][\"value\"]";
      return -6;
    }

    myKey temp;
    temp.key = structValue["key"].toString();
    temp.value = structValue["value"].toString();
    if (structValue["operate"].valid())
      temp.operate = structValue["operate"].toString();
    else
      temp.operate = "=";
    keyList.push_back(temp);
  }
  //unisigned->signed 溢出,考虑做范围约定
  int tmpsize = keyList.size();
  return tmpsize;
}

void BsrSQL::checkNodeDel(XmlRpcValue& params, XmlRpcValue& result, const string& table, const string& key)
{
  if (!params["id"].valid())
  {
    throw XmlRpcException("Not found param id for nodeIsEmpty", ERR_MYSQL + 1);
  }
  if (getType(params["id"]) != XmlRpc::XmlRpcValue::TypeInt)
  {
    throw XmlRpcException("param id is not integer for nodeIsEmpty", ERR_MYSQL + 1);
  }
  int id = (int)params["id"];
  string sql = "SELECT id FROM " + table + " WHERE ";
  sql += key + "='" + toStr<int>(id) + "'";
  int num = 0;
  Result res = store(sql, &num);
  if (!res)
  {
    throw XmlRpcException("can't get result, delete fail", ERR_MYSQL + 2);
  }
  if (num > 0)
  {
    throw XmlRpcException("子结点非空，删除失败", ERR_MYSQL + 3);
  }

}


/**
* @fn checkParams
* @brief 获取客户端传过来的关键字和值
* @param[in] params  传入的客户端参数.
* @param[out]  keyword   客户端传过来的关键字，
如果有参数bkeyword指定了关键字，则keyword为指定关键字
如果有参数bkeyword没有指定关键字，则"id"作为默认关键字
* @param[out]  value 存放keyword的值
* @return    返回params包含的参数的个数
* @retval  >:  OK
* @retval  <=0:  参数错误
*/
int BsrSQL::checkParams(XmlRpcValue& params, string &keyword, string &value)
{
  if (params.valid())
  {
    if (getType(params) != XmlRpc::XmlRpcValue::TypeStruct)
      return -1;
    getParamList(params);
  }
  else
  {
    return 0;
  }

  int size = m.size();
  if (size <= 0)
    return 0;

  keyword = "id";
  for (int i = 0; i < size; i++)
  {
    if (m[i] == "bkeyword")
    {
      string k = params["bkeyword"];
      keyword = k;
      break;
    }
  }

  if (getType(params[keyword]) == XmlRpc::XmlRpcValue::TypeInt)
  {
    value = params[keyword].toString();
  }
  else if (getType(params[keyword]) == XmlRpc::XmlRpcValue::TypeString)
  {
    string c = params[keyword].toString();
    value = c;
  }
  else
  {
    value = "";
  }

  return size;
}




/**
* @fn exec
* @brief  通过该函数可以执行一指定的sql语句，调用该函数前需先调用set_sql函数设定sql语句
*      该函数一般用于insert,delete,update
* @param[in] 通过set_sql设定一sql语句
* @param[out] result 如果执行成功,通过下标"rows"返回影响的记录数,
如果执行失败，抛出mysql异常
如果使用该函数执行select语句，只返回记录数，不返回结果集
* @see load()
*/
bool BsrSQL::exec(XmlRpcValue& result)
{
  AutoMutex mutexDestory(mutex);			//线程同步
  m_res = store(m_sql, &m_affectedNum);
  if (m_affectedNum >= 0)
  {
    result["count"] = m_affectedNum;
    return true;
  }
  else
  {
    LOG_DEBUG(NOTHREAD, "sql:%s, mysql error:%s", m_sql.c_str(), m_con->error());
    return false;
  }
}



/**
* @fn load
* @brief  通过该函数可以执行一指定的sql语句，调用该函数前需先调用set_sql函数设定sql语句
*      该函数一般用于select
* @param[in] 通过set_sql设定一sql语句
* @param[out] result 如果执行成功,通过下标"rows"返回影响的记录数,通过数据表下标返回结果数据
如果执行失败，抛出mysql异常
如果使用该函数执行insert,update,delete语句，只返回记录数
* @see exec()
*/
bool BsrSQL::load(XmlRpcValue& result)
{
  m_res = store(m_sql, &m_affectedNum);

  if (m_affectedNum < 0)
  {
    LOG_DEBUG(NOTHREAD, "sql:%s, mysql error:%s", m_sql.c_str(), m_con->error());
    return false;
  }
  if (m_res)
  {
    int cols = m_res.columns();
    int rows = m_res.size();
    if (rows > 0)
    {
      // 如果count值不存在,则置count表示查询的结果数
      if (!result["count"].valid())
        result["count"] = rows;

      Row row;
      int j = 0;
      while (row = m_res.fetch_row())
      {
        for (int i = 0; i < cols; i++)
        {
          string field = m_res.field_name(i);
          //result["rows"][r][field] = (string)row[i];    
          mysql_type_info t(m_res.fields(i));

          string sqlName = t.sql_name();
          if (mysqlTimestamp(sqlName) || mysqlChar(sqlName) || mysqlDatetime(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (string)row[i];
            else
              result["rows"][j][field] = "";
          }
          else if (mysqlInt(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (int)row[i];
            else
              result["rows"][j][field] = 0;
          }
          else if (mysqlFloatDouble(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (double)row[i];
            else
              result["rows"][j][field] = 0.000000;
          }
          else
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (string)row[i];
            else
              result["rows"][j][field] = "";
          }
        }
        j++;
      }
    }
    else
    {
      if (!result["count"].valid())
        result["count"] = 0;
    }

  }
  return true;
}


/**
* @fn add
* @brief 根据传入参数增加一个记录到指定的数据表
* @param[in] params  客户端传入的参数
* @param[in] table   操作的数据表名
* @param[out] result 执行成功返回1，失败返回异常
* @see edit show del list
*/
void BsrSQL::myadd(XmlRpcValue& params, XmlRpcValue& result, const string& table)
{
  reset();
  if (!params.valid() || !getParamList(params))
  {
    throw XmlRpcException("Not found param for add()", ERR_MYSQL + 4);
    return;
  }

  int size = m.size();
  m_sql = "INSERT INTO " + table + " SET";
  for (int i = 0; i < size; i++)
  {
    string v = "";
    /*
    if(getType( params[m[i]]) == XmlRpc::XmlRpcValue::TypeInt)
    {
      v = params[m[i]].toString();
    }
    else
    {
      string c = params[m[i]];
      v = c;
    }
    */
    v = params[m[i]].toString();
    m_sql += " " + m[i] + "='" + v + "',";
  }
  m_sql = m_sql.substr(0, m_sql.length() - 1);

  LOG_DEBUG(NOTHREAD, "BsrSQL::add(): %s", m_sql.c_str());

  m_res = store(m_sql, &m_affectedNum);
  if (m_affectedNum >= 0)
  {
    int id = m_con->insert_id();
    result["id"] = id;
    result["count"] = m_affectedNum;
  }
  else
  {
    throw XmlRpcException("add fail", ERR_MYSQL + 5);
  }
}



/**
* @fn edit
* @brief  根据传入参数id更新数据表记录，
* @param[in] params  客户端传入的参数
* @param[in] table   操作的数据表名
* @param[out] result 执行成功返回受影响的记录数，失败返回异常
* @see edit show del list
*/
void BsrSQL::myedit(XmlRpcValue& params, XmlRpcValue& result, const string& table)
{
  reset();
  if (!params.valid() || !getParamList(params))
    throw XmlRpcException("Not found param for edit()", ERR_MYSQL);

  if (!params["id"].valid())
    throw XmlRpcException("param id is invalid for edit()", ERR_MYSQL);

  string keyword = "id";
  string value = params["id"].toString();
  m_sql = "UPDATE " + table + " SET";
  int size = m.size();
  for (int i = 0; i < size; i++)
  {
    string v = "";
    v = params[m[i]].toString();
    m_sql += " " + m[i] + "='" + v + "',";

  }
  m_sql = m_sql.substr(0, m_sql.length() - 1);
  m_sql += " WHERE " + keyword + "='" + value + "'";
  LOG_DEBUG(NOTHREAD, "BsrSQL::edit(): %s", m_sql.c_str());

  /// exec sql
  m_res = store(m_sql, &m_affectedNum);
  if (m_affectedNum >= 0)
  {
    result["count"] = m_affectedNum;
  }
  else
    throw XmlRpcException("edit fail", ERR_MYSQL);

}


/**
* @fn del
* @brief  根据传入参数id删除数据表记录，
* @param[in] params  客户端传入的参数
* @param[in] table   操作的数据表名
* @param[out] result 执行成功返回受影响的记录数，失败返回异常
* @see edit show del list
*/
void BsrSQL::mydel(XmlRpcValue& params, XmlRpcValue& result, const string& table)
{
  reset();

  if (!params.valid() || !getParamList(params))
  {
    throw XmlRpcException("Not found param for del()", ERR_MYSQL);
  }
  if (!params["id"].valid())
  {
    throw XmlRpcException("param id is invalid for del()", ERR_MYSQL);
  }

  string keyword = "id";
  string value = params["id"].toString();
  m_sql = "DELETE from " + table + " WHERE " + keyword + "='" + value + "'";
  LOG_DEBUG(NOTHREAD, "BsrSQL::del(): %s", m_sql.c_str());
  m_res = store(m_sql, &m_affectedNum);
  if (m_affectedNum >= 0)
  {
    result["count"] = m_affectedNum;
  }
  else
  {
    throw XmlRpcException("delete fail", ERR_MYSQL);
  }

}


/**
* @fn mylist
* @brief  根据传入参数查询数据表记录，该函数为list提供接口
* @param[in] params  客户端传入的参数
如果参数bkeyword为空，默认的keyword为id
如果参数中没有指定bkeyword, 且有参数id,且id的值为字符串"all"，则返回所有信息
如果参数bkeyword不为空，按bkeyword指定的关键词进行查询
如果参数中包含pageNumber且值为0，或不包含page, 则返回所有的结果集
如果参数中包含pageNumber且值大于0，则返回指定的页的结果集
如果参数中包含pageNumber且值大于0，如果pageRows没有指定，则默认每页返回的结果集为defaultPageRows条，否则按pageRows指定的返回
* @param[in] table   操作的数据表名
* @param[out] result 执行成功通过下标"rows"返回受影响的记录数,通过数据表下标返回结果数据
如果失败返回异常
*/
void BsrSQL::mylist(XmlRpcValue& params, XmlRpcValue& result, const string& table)
{
  reset();
  bool paramIsNotValid = false;
  string keyword = "";
  string value = "";
  int size = checkParams(params, keyword, value);
  if (size < 0)
    throw XmlRpcException("Param type must be struct for mylist()", ERR_MYSQL);

  if (size == 0)
    paramIsNotValid = true;

  if (value.empty())
  {
    throw XmlRpcException("the keyword not value for mylist() and " + keyword, ERR_MYSQL);
  }

  /// check pageNumber and pageRows
  int pageNumber = global::defaultPageNumber;
  int pageRows = global::defaultPageRows;
  if (getType(params["pageNumber"]) == XmlRpc::XmlRpcValue::TypeInt)
  {
    pageNumber = params["pageNumber"];
  }
  else
  {
    pageNumber = global::defaultPageNumber;
  }

  if (pageNumber < 0 || pageNumber > global::defaultMaxPageNumber)
  {
    throw XmlRpcException("pageNumber invalid range for mylist()", ERR_MYSQL);
    return;
  }

  if (pageNumber != 0)
  {
    if (getType(params["pageRows"]) == XmlRpc::XmlRpcValue::TypeInt)
    {
      pageRows = params["pageRows"];
    }
    else
    {
      pageRows = global::defaultPageRows;
    }

    if (pageRows < 0 || pageRows > global::defaultMaxPageRows)
    {
      throw XmlRpcException("pageRows invalid range for mylist()", ERR_MYSQL);
      return;
    }
  }

  Row row;
  char sql[1024];
  if (paramIsNotValid)
  {
    snprintf(sql, sizeof(sql), "SELECT * FROM %s", table.c_str());
    m_res = store(sql, &m_affectedNum);
    result["count"] = m_affectedNum;
    result["pageRows"] = m_affectedNum;
    result["pageNumber"] = result["pageCount"] = 1;
    reset();
  }
  else
  {
    if (pageNumber != 0)
    {
      snprintf(sql, sizeof(sql), "SELECT * FROM %s WHERE %s='%s' LIMIT %d,%d", table.c_str(), keyword.c_str(), value.c_str(), pageNumber - 1, pageRows);
      /// calculate page info
      char sql2[1024];
      snprintf(sql2, sizeof(sql2), "SELECT * FROM %s WHERE %s='%s'", table.c_str(), keyword.c_str(), value.c_str());
      m_res = store(sql2, &m_affectedNum);
      result["count"] = m_affectedNum;
      result["pageNumber"] = pageNumber;
      result["pageRows"] = pageRows;
      if ((m_affectedNum % pageRows) != 0)
        result["pageCount"] = ((int)result["count"] / (int)result["pageRows"]) + 1;
      else
        result["pageCount"] = (int)result["count"] / (int)result["pageRows"];

      reset();
    }
    else
    {
      snprintf(sql, sizeof(sql), "SELECT * FROM %s WHERE %s='%s'", table.c_str(), keyword.c_str(), value.c_str());
      /// calculate page info
      char sql2[1024];
      snprintf(sql2, sizeof(sql2), "SELECT * FROM %s WHERE %s='%s'", table.c_str(), keyword.c_str(), value.c_str());
      m_res = store(sql2, &m_affectedNum);
      //int c = (int)(m_res.size());
      result["count"] = m_affectedNum;
      result["pageRows"] = m_affectedNum;
      result["pageNumber"] = result["pageCount"] = 1;

      reset();
    }
  }

  m_sql = sql;
  LOG_DEBUG(NOTHREAD, "BsrSQL: %s", m_sql.c_str());
  /// exec sql

  m_res = store(m_sql, &m_affectedNum);
  if (m_res)
  {
    int cols = m_res.columns();
    int rows = m_res.size();
    if (rows > 0)
    {
      int j = 0;
      while (row = m_res.fetch_row())
      {
        for (int i = 0; i < cols; i++)
        {
          string field = m_res.field_name(i);
          mysql_type_info t(m_res.fields(i));

          string sqlName = t.sql_name();
          if (mysqlTimestamp(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (string)row[i];
            else
              result["rows"][j][field] = "";
          }
          else if (mysqlInt(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (int)row[i];
            else
              result["rows"][j][field] = 0;
          }
          else if (mysqlChar(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (string)row[i];
            else
              result["rows"][j][field] = "";
          }
          else if (mysqlDatetime(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (string)row[i];
            else
              result["rows"][j][field] = "";
          }
          else if (mysqlFloatDouble(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (double)row[i];
            else
              result["rows"][j][field] = 0.000000;
          }
          else
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (string)row[i];
            else
              result["rows"][j][field] = "";
          }

        }
        j++;
      } //while(row = m_res.fetch_row())
    }  //if(rows > 0)  
  } //if(m_res)
}




/**
* @fn myshow
* @brief  根据传入参数id查询一条数据表记录，
* @param[in] params  客户端传入的参数，必须包含id,负责返回错误信息
* @param[in] table   操作的数据表名
* @param[out] result 执行成功通过数据表下标返回结果数据
如果失败返回异常
*/

void BsrSQL::myshow(XmlRpcValue& params, XmlRpcValue& result, const string& table)
{
  reset();

  if (!params.valid() || !getParamList(params))
  {
    throw XmlRpcException("Not found param for myshow()", ERR_MYSQL + 6);
  }
  if (!params["id"].valid())
    throw XmlRpcException("param id is invalid for myshow()", ERR_MYSQL + 6);

  string keyword = "id";
  string value = params["id"].toString();
  Row row;
  char sql[1024];
  snprintf(sql, sizeof(sql), "SELECT * FROM %s WHERE %s='%s'", table.c_str(), keyword.c_str(), value.c_str());

  m_sql = sql;
  LOG_DEBUG(NOTHREAD, "BsrSQL::myshow(): %s", m_sql.c_str());
  m_res = store(m_sql, &m_affectedNum);
  if (m_res)
  {
    int cols = m_res.columns();
    int rows = m_res.size();
    if (rows == 1)
    {
      row = m_res.fetch_row();
      for (int i = 0; i < cols; i++)
      {
        string field = m_res.field_name(i);
        mysql_type_info t(m_res.fields(i));
        //cout << field << ",";
        //cout << t.sql_name() << ",";
        //cout << t.name() << "," << endl;

        string sqlName = t.sql_name();
        if (mysqlTimestamp(sqlName))
        {
          if (!row[i].is_null())
            result[field] = (string)row[i];
          else
            result[field] = "";
        }
        else if (mysqlInt(sqlName))
        {
          if (!row[i].is_null())
            result[field] = (int)row[i];
          else
            result[field] = 0;
        }
        else if (mysqlChar(sqlName))
        {
          if (!row[i].is_null())
            result[field] = (string)row[i];
          else
            result[field] = "";
        }
        else if (mysqlDatetime(sqlName))
        {
          if (!row[i].is_null())
            result[field] = (string)row[i];
          else
            result[field] = "";
        }
        else if (mysqlFloatDouble(sqlName))
        {
          if (!row[i].is_null())
            result[field] = (double)row[i];
          else
            result[field] = 0.000000;
        }
        else
        {
          if (!row[i].is_null())
            result[field] = (string)row[i];
          else
            result[field] = "";
        }
      }
      result["count"] = 1;
      result["pageNumber"] = 1;
      result["pageRows"] = global::defaultPageRows;
      result["pageCount"] = 1;
    }
    else
    {
      result["count"] = 0;
      result["pageNumber"] = 1;
      result["pageRows"] = global::defaultPageRows;
      result["pageCount"] = 0;
    }
  }

}

/**
* @fn multilist
* @brief  根据传入参数查询数据表记录，该函数为list提供接口
* @param[in] params  客户端传入的参数
如果参数bkeyword为空，默认的keyword为id
如果参数中没有指定bkeyword, 且有参数id,且id的值为字符串"all"，则返回所有信息
如果参数bkeyword不为空，按bkeyword指定的关键词进行查询
如果参数中包含pageNumber且值为0，或不包含page, 则返回所有的结果集
如果参数中包含pageNumber且值大于0，则返回指定的页的结果集
如果参数中包含pageNumber且值大于0，如果pageRows没有指定，则默认每页返回的结果集为defaultPageRows条，否则按pageRows指定的返回
* @param[in] table   操作的数据表名
* @param[out] result 执行成功通过下标"rows"返回受影响的记录数,通过数据表下标返回结果数据
如果失败返回异常
*/
void BsrSQL::multilist(XmlRpcValue& params, XmlRpcValue& result, const string& table, const string& sortstr)
{
  reset();
  bool paramIsNotValid = false;
  bool fieldIsValid = false;
  bool condIsValid = false;
  vector<myKey> keyList;
  int condNumber = 0;
  if (params.valid())
  {
    fieldIsValid = getParamList(params);
    condNumber = getCondition(params, keyList);
    if (condNumber < 0)
    {
      throw XmlRpcException(m_errorStr, m_errorCode);
    }
    else if (condNumber > 0)
      condIsValid = true;
    else
      paramIsNotValid = true;
  }
  else
  {
    paramIsNotValid = true;
  }

  if (!condIsValid && params["id"].valid())
  {
    myKey temp;
    temp.key = "id";
    temp.value = params["id"].toString();
    temp.operate = "=";
    keyList.push_back(temp);
    condIsValid = true;
    condNumber = keyList.size();
  }

  /// check pageNumber and pageRows
  int pageNumber = global::defaultPageNumber;
  int pageRows = global::defaultPageRows;
  if (getType(params["pageNumber"]) == XmlRpc::XmlRpcValue::TypeInt)
  {
    pageNumber = params["pageNumber"];
  }
  else
  {
    pageNumber = global::defaultPageNumber;
  }

  if (pageNumber < 0 || pageNumber > global::defaultMaxPageNumber)
  {
    throw XmlRpcException("pageNumber invalid range for multilist()", ERR_MYSQL + 13);
  }

  if (pageNumber != 0)
  {
    if (getType(params["pageRows"]) == XmlRpc::XmlRpcValue::TypeInt)
    {
      pageRows = params["pageRows"];
    }
    else
    {
      pageRows = global::defaultPageRows;
    }

    if (pageRows < 0 || pageRows > global::defaultMaxPageRows)
    {
      throw XmlRpcException("pageRows invalid range for multilist()", ERR_MYSQL + 14);
    }
  }

  Row row;
  string sqlRes;
  string sqlPage;

  if (paramIsNotValid)
  {
    sqlRes = "SELECT * FROM " + table;
    sqlPage = sqlRes;
    sqlRes += " order by " + sortstr;
  }
  else if (condIsValid)
  {
    sqlRes = "SELECT * FROM " + table + " WHERE 1=1";
    for (int i = 0; i < condNumber; ++i)
    {
      string tmpStr = "";
      if (keyList[i].operate == "like")
      {
        tmpStr = keyList[i].key + " like " + "'%" + keyList[i].value + "%'";
      }
      else if (keyList[i].operate == "rlike")
      {
        tmpStr = keyList[i].key + " like " + "'%" + keyList[i].value + "'";
      }
      else if (keyList[i].operate == "llike")
      {
        tmpStr = keyList[i].key + " like " + "'" + keyList[i].value + "%'";
      }
      else
      {
        tmpStr = keyList[i].key + keyList[i].operate + "'" + keyList[i].value + "'";
      }
      sqlRes += " AND " + tmpStr;
    }
    sqlPage = sqlRes;
    sqlRes += " order by " + sortstr;

    if (pageNumber != 0)
    {
      char tmpstr[256];
      snprintf(tmpstr, sizeof(tmpstr), " LIMIT %d,%d", pageNumber - 1, pageRows);
      sqlPage += tmpstr;
    }
  }
  else
  {
    throw XmlRpcException("Can't identify condition for multilist()", ERR_MYSQL + 15);
  }

  /// 计算页的信息
  m_res = store(sqlPage, &m_affectedNum);
  result["count"] = m_affectedNum;
  result["pageNumber"] = pageNumber;
  result["pageRows"] = pageRows;
  //可能有问题
  result["pageCount"] = (int)ceil((double)m_affectedNum / pageRows);
  reset();

  /// 执行sql
  m_sql = sqlRes;
  m_res = store(m_sql, &m_affectedNum);
  if (m_res)
  {
    int cols = m_res.columns();
    int rows = m_res.size();
    if (rows > 0)
    {
      int j = 0;
      while (row = m_res.fetch_row())
      {
        for (int i = 0; i < cols; i++)
        {
          string field = m_res.field_name(i);
          mysql_type_info t(m_res.fields(i));

          string sqlName = t.sql_name();
          if (mysqlTimestamp(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (string)row[i];
            else
              result["rows"][j][field] = "";
          }
          else if (mysqlInt(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (int)row[i];
            else
              result["rows"][j][field] = 0;
          }
          else if (mysqlChar(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (string)row[i];
            else
              result["rows"][j][field] = "";
          }
          else if (mysqlDatetime(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (string)row[i];
            else
              result["rows"][j][field] = "";
          }
          else if (mysqlFloatDouble(sqlName))
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (double)row[i];
            else
              result["rows"][j][field] = 0.000000;
          }
          else
          {
            if (!row[i].is_null())
              result["rows"][j][field] = (string)row[i];
            else
              result["rows"][j][field] = "";
          }

        }
        j++;
      } //while(row = m_res.fetch_row())
    }  //if(rows > 0)
  } //if(m_res)
}



/**
* @fn multiedit
* @brief  根据传入参数条件更新数据表记录，注意如果不指定条件则更新所有记录
* @param[in] params  客户端传入的参数
* @param[in] table   操作的数据表名
* @param[out] result 执行成功通过下标"count"返回受影响的记录数,通过数据表下标返回结果数据
如果失败返回异常
*/
void BsrSQL::multiedit(XmlRpcValue& params, XmlRpcValue& result, const string& table)
{
  reset();

  bool paramIsNotValid = false;
  bool fieldIsValid = false;
  bool condIsValid = false;
  vector<myKey> keyList;
  int condNumber = 0;
  if (params.valid())
  {
    fieldIsValid = getParamList(params);
    if (!fieldIsValid)
    {
      throw XmlRpcException("field param is empty for multiedit()", ERR_MYSQL + 16);
    }
    condNumber = getCondition(params, keyList);
    if (condNumber < 0)
    {
      throw XmlRpcException(m_errorStr, m_errorCode);
    }
    else if (condNumber > 0)
      condIsValid = true;
  }
  else
  {
    paramIsNotValid = true;
    throw XmlRpcException("param is empty for multiedit()", ERR_MYSQL + 17);
  }

  if (!condIsValid)
  {
    if (!params["id"].valid())
    {
      throw XmlRpcException("_con and id is empty for multiedit()", ERR_MYSQL + 18);
    }
    else
    {
      myKey temp;
      temp.key = "id";
      temp.value = params["id"].toString();
      temp.operate = "=";
      keyList.push_back(temp);
      condIsValid = true;
      condNumber = keyList.size();
    }
  }

  int idvalue = 0;
  int fieldSize = m.size();
  m_sql = "UPDATE " + table + " SET";
  for (int i = 0; i < fieldSize; ++i)
  {
    if (m[i] == "id")
    {
      idvalue = (int)params[m[i]];
      continue;
    }
    string v = "";
    v = params[m[i]].toString();
    m_sql += " " + m[i] + "='" + v + "',";
  }
  m_sql = m_sql.substr(0, m_sql.length() - 1);
  m_sql += " WHERE 1=1";
  if (condIsValid)
  {
    for (int i = 0; i < condNumber; ++i)
    {
      m_sql += " AND " + keyList[i].key + keyList[i].operate + "'" + keyList[i].value + "'";
    }
  }
  LOG_DEBUG(NOTHREAD, "BsrSQL::multiedit(): %s", m_sql.c_str());
  m_res = store(m_sql, &m_affectedNum);
  if (m_affectedNum > 0)
    result["count"] = m_affectedNum;
  else if (m_affectedNum == 0)
    result["count"] = 0;
  //throw XmlRpcException("the revisable information has not changed", ERR_MYSQL+19);
  else
    throw XmlRpcException("Ip possible conflicts or other errors,modify fail", ERR_MYSQL + 20);

  if (idvalue > 0)
    result["id"] = idvalue;
}


/**
* @fn multidel
* @brief  根据传入参数条件删除数据表记录，注意如果不指定条件则删除所有记录
* @param[in] params  客户端传入的参数
* @param[in] table   操作的数据表名
* @param[out] result 执行成功通过下标"count"返回受影响的记录数,通过数据表下标返回结果数据
如果失败返回异常
*/
void BsrSQL::multidel(XmlRpcValue& params, XmlRpcValue& result, const string& table)
{
  reset();

  bool paramIsNotValid = false;
  bool fieldIsValid = false;
  bool condIsValid = false;
  vector<myKey> keyList;
  int condNumber = 0;
  if (params.valid())
  {
    fieldIsValid = getParamList(params);
    condNumber = getCondition(params, keyList);
    if (condNumber < 0)
    {
      throw XmlRpcException(m_errorStr, m_errorCode);
    }
    else if (condNumber > 0)
      condIsValid = true;
  }
  else
  {
    paramIsNotValid = true;
    //throw XmlRpcException("param is empty for multidel()", ERR_MYSQL);
  }

  if (!condIsValid)
  {
    if (!params["id"].valid())
    {
      throw XmlRpcException("_con and id is empty for multidel()", ERR_MYSQL + 21);
    }
    else
    {
      myKey temp;
      temp.key = "id";
      temp.value = params["id"].toString();
      temp.operate = "=";
      keyList.push_back(temp);
      condIsValid = true;
      condNumber = keyList.size();
    }
  }

  m_sql = "DELETE from " + table + " WHERE 1=1";
  if (condIsValid)
  {
    for (int i = 0; i < condNumber; ++i)
    {
      m_sql += " AND " + keyList[i].key + keyList[i].operate + "'" + keyList[i].value + "'";
    }
  }
  LOG_DEBUG(NOTHREAD, "BsrSQL::multiedit(): %s", m_sql.c_str());
  m_res = store(m_sql, &m_affectedNum);
  if (m_affectedNum > 0)
    result["count"] = m_affectedNum;
  else if (m_affectedNum == 0)
    result["count"] = 0;
  //throw XmlRpcException("delete fail", ERR_MYSQL+22);
  else
    throw XmlRpcException("database exception,delete fail", ERR_MYSQL + 23);

}

/**
* @fn list show add edit del
* @brief  根据传入参数操作数据表记录，
* @param[in] params  客户端传入的参数
* @param[in] table   操作的数据表名
* @param[out] result 执行成功通过下标"rows"返回受影响的记录数,通过数据表下标返回结果数据
如果失败返回异常
*/
void BsrSQL::list(XmlRpcValue& params, XmlRpcValue& result, const string& table, const string& sortstr)
{
  multilist(params, result, table, sortstr);
}

void BsrSQL::edit(XmlRpcValue& params, XmlRpcValue& result, const string& table)
{
  multiedit(params, result, table);
}

void BsrSQL::del(XmlRpcValue& params, XmlRpcValue& result, const string& table)
{
  multidel(params, result, table);
}

void BsrSQL::show(XmlRpcValue& params, XmlRpcValue& result, const string& table)
{
  myshow(params, result, table);
}

void BsrSQL::add(XmlRpcValue& params, XmlRpcValue& result, const string& table)
{
  myadd(params, result, table);
}



/**
* @fn show
* @brief  根据传入参数id查询数据表记录
* @param[in] id   数据表对应的id值
* @param[in] table   操作的数据表名
* @param[out] result 执行成功通过下标"count"返回受影响的记录数,通过数据表下标返回结果数据
如果失败返回异常
* @see select
*/
void BsrSQL::show(int id, XmlRpcValue& result, string table)
{
  XmlRpcValue params;
  params["id"] = id;
  myshow(params, result, table);
}

bool BsrSQL::query(const string& sql, mysqlpp::Result& res, int& nRows)
{
  AutoMutex mutexDestory(mutex);				//线程同步
  int i = 0;
redo:
  try {
    mysqlpp::Query query(m_con);
    query << sql;
    res = query.store();
    nRows = (int)(m_con->affected_rows());

    return true;
  }
  catch (const mysqlpp::BadQuery& er) {
    cerr << "Query error: \"" << er.what() << "\"" << endl;
    string errstr = er.what();
    if (errstr == "Lost connection to MySQL server during query" && ++i <= 1) {
      goto redo;
    }
    return false;
  }
  catch (const mysqlpp::Exception& er) {
    // Catch-all for any other MySQL++ exceptions
    cerr << "Error: " << er.what() << endl;
    return false;
  }
}


bool BsrSQL::query(mysqlpp::Connection& con, string sql, mysqlpp::Result& res, int& nRows)
{
  int i = 0;
redo:
  try {
    mysqlpp::Query query(&con);
    // query.disable_exceptions();
    query << sql;
    res = query.store();
    nRows = (int)(con.affected_rows());

    return true;
  }
  catch (const mysqlpp::BadQuery& er) {
    cerr << "Query error: \"" << er.what() << "\"" << endl;
    string errstr = er.what();
    if (errstr == "Lost connection to MySQL server during query" && ++i <= 1) {
      goto redo;
    }
    return false;
  }
  catch (const mysqlpp::Exception& er) {
    // Catch-all for any other MySQL++ exceptions
    cerr << "Error: " << er.what() << endl;
    return false;
  }
}

bool BsrSQL::execute(const string &sqlStr, int *numRows)
{
  AutoMutex mutexDestory(mutex);			//线程同步
  int i = 0;
redo:
  try {
    ResNSel res;
    Query query = m_con->query();
    query << sqlStr;
    res = query.execute();
    *numRows = (int)(m_con->affected_rows());
    return bool(res);
  }

  catch (const mysqlpp::BadQuery& er) {
    cerr << "Query error: \"" << er.what() << "\"" << endl;
    string errstr = er.what();
    if (errstr == "Lost connection to MySQL server during query" && ++i <= 3) {
      sleep(1);
      goto redo;
    }
    return false;
  }
  catch (const mysqlpp::Exception& er) {
    // Catch-all for any other MySQL++ exceptions
    cerr << "Error: " << er.what() << endl;
    return false;
  }
}

