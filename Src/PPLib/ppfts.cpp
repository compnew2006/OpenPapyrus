// PPFTS.CPP
// Copyright (c) A.Sobolev 2022
// @codepage UTF-8
// @construction
// For this module compiler option /EHsc must be set (Enable C++ exceptions)
// Полнотекстовый поиск
//
#include <pp.h>
#pragma hdrstop

#include <unordered_map>
#include <unordered_set>
#include <dbkv-lmdb.h>

//int  Test_Fts() { return 1; }
#if 1 // {

#include <xapian.h>
#include <unicode\uclean.h>
#include <unicode\brkiter.h>
//
// Xapian interface development
//
using namespace U_ICU_NAMESPACE;
//
// Descr: Класс, управляющий базой данных полнотекстового поиска
//
class PPFtsDatabase {
public:
	enum { // @persistent
		scopeUndef     = 0, // Не определен
		scopePPDb      = 1, // База данных Papyrus 
		scopeStyloQSvc = 2, // Сервис Stylo-Q
	};
	struct Entity {
		Entity();
		Entity(const Entity & rS);
		Entity & FASTCALL operator = (const Entity & rS);
		bool   FASTCALL operator == (const Entity & rS) const { return IsEq(rS); }
		Entity & Z();

		bool   FASTCALL IsEq(const Entity & rS) const;
		bool   IsValid() const;
		//int    Serialize(int dir, SBuffer & rBuf, SSerializeContext * pSCtx);
		int    MakeSurrogateScopeIdent(SBinaryChunk & rSsi) const;
		int    SetSurrogateScopeIdent(const SBinaryChunk & rSsi);

		uint32 Scope;
		uint32 ObjType;
		uint64 ObjId;
		SString ScopeIdent;
	};
	struct SearchResultEntry : public Entity {
		SearchResultEntry() : Entity(), DocId(0), Rank(0), Weight(0.0)
		{
		}
		uint64 DocId;
		uint64 Rank;
		double Weight;
	};
	explicit PPFtsDatabase(bool forUpdate);
	~PPFtsDatabase();
	//
	// Примерный сценарий индексации:
	// {
	//    PPFtsDatabase db(true/*for_update*/);
	//    SHandle tra = db.BeginTransaction();
	//    for(uint i = 0; i < doc_count; i++) {
	//       PPFtsDatabase::Entity entity = ...; // Расширенный идентификатор документа
	//       StringSet ss = ...; // Набор термов документа в кодировке UTF8
	//       db.PutEntry(tra, entity, ss);
	//    }
	//    db.CommitTransaction(tra);
	// }
	//
	SHandle BeginTransaction();
	int    CommitTransaction(SHandle);
	int    AbortTransaction(SHandle);
	//
	// Returns:
	//   >0 - идентификатор документа в базе данных
	//    0 - ошибка
	//
	uint64 PutEntity(SHandle transation, Entity & rEnt, StringSet & rSsUtf8);
	int    Search(const char * pQueryUtf8, TSCollection <SearchResultEntry> & rResult);
private:
	int    GetEntityKey(Entity & rEnt, uint64 * pSurrogateScopeIdent, SBuffer & rEntityBuf, uint64 * pKey);
	int    StoreEntityKey(uint64 surrogateScopeIdent, const SBuffer & rEntityBuf, uint64 key);
	int    SearchEntityKey(uint64 key, Entity & rEnt, uint64 * pSurrogateScopeIdent);
	//
	// Descr: Класс, управляющий комбинированной транзакцией изменения индекса.
	//
	class Transaction {
	public:
		Transaction(PPFtsDatabase & rMaster);
		~Transaction();
		bool   IsValid() const;
		bool   Commit();
		bool   Abort();
		LmdbDatabase::Table * GetDomainT();
		LmdbDatabase::Table * GetDomainRevT();
		LmdbDatabase::Table * GetKeyT();
		LmdbDatabase::Table * GetKeyRevT();
	private:
		void   DestroyTables();
		enum {
			stError    = 0x0001,
			stFinished = 0x0002
		};
		uint   State;
		PPFtsDatabase & R_Master;
		LmdbDatabase::Transaction * P_STxn;
		//
		// Так как мы предполагаем индексировать большое число мелких объектов (товары, персоналии, документы и т.д.)
		// то длина комбинированного идентификатора каждого такого объекта оказывается слишком большой.
		// Для того, чтобы сократить издержки идентификации, мы будем хранить такие идентификаторы в двух (с учетом реверсной индексации - в четырех)
		// таблицах: 
		// 1. Идентификаторы доменов {Entity::Scope; Enity::ScopeIdent}-->SurrogateDomainIdent
		// 2. Идентификаторы сущностей внутри доменов {SurrogateDomainIdent; ObjType; ObjId}
		//
		LmdbDatabase::Table * P_SDomainT;  // Таблица доменных частей индексации: Domain->SurrogateIdent
		LmdbDatabase::Table * P_SDomainRevT; // Таблица обратных ключей доменных частей индексации: SurrogateIdent->Domain
		LmdbDatabase::Table * P_SKeyT;    // Таблица ключей индексации: Entity->XapianID
		LmdbDatabase::Table * P_SKeyRevT; // Таблица обратных ключей индексации: XapianID->Entity
	};
	enum {
		dbdMain         = 1,
		dbdSupplemental = 2
	};
	int    GetDatabasePath(int dbd, SString & rBuf) const;
	enum {
		stError   = 0x0001,
		stWriting = 0x0002  // База данных открыта для изменений //
	};
	uint   State;
	Xapian::Database * P_XDb;
	LmdbDatabase * P_SupplementalDb; // @not owned
	Transaction * P_CurrentTra;
};

int PPFtsDatabase::Search(const char * pQueryUtf8, TSCollection <SearchResultEntry> & rResult)
{
	int    ok = -1;
	SHandle tra;
	//SString temp_buf;
	if(!isempty(pQueryUtf8)) {
		SString query_buf(pQueryUtf8);
		THROW(query_buf.IsLegalUtf8());
		query_buf.Utf8ToLower();
		{
			tra = BeginTransaction();
			THROW(tra);
			assert(P_CurrentTra && P_CurrentTra == tra);
			assert(P_XDb);
			{
				SStringU qb_u;
				std::string found_doc_text;
				//Xapian::Database db("xapian-test-db", Xapian::DB_OPEN);
				Xapian::Enquire enq(*P_XDb);
				Xapian::QueryParser qp;
				SSerializeContext sctx;
				SBuffer sbuf_entity;
				LmdbDatabase::Table * p_key_rev_tbl = P_CurrentTra->GetKeyRevT();
				assert(p_key_rev_tbl); // Выше мы успешно начали транзакцию: не может быть, что p_key_rev_tbl == 0
				qp.set_database(*P_XDb);
				qb_u.CopyFromUtf8(query_buf);
				Xapian::Query q = qp.parse_query(query_buf.cptr());
				std::string qdescr = q.get_description();
				//
				enq.set_query(q);
				Xapian::MSet ms = enq.get_mset(0, 10);
				for(Xapian::MSetIterator i = ms.begin(); i != ms.end(); ++i) {
					//Xapian::doccount rank = i.get_rank();
					//double wt = i.get_weight();
					//Xapian::docid did = *i;
					found_doc_text = i.get_document().get_data();
					//cout << i.get_rank() + 1 << ": " << i.get_weight() << " docid=" << *i << " [" << i.get_document().get_data() << "]\n\n";			
					uint64 surrogate_scope_id = 0;
					SearchResultEntry * p_re = rResult.CreateNewItem();
					p_re->DocId = *i;
					p_re->Rank = i.get_rank();
					p_re->Weight = i.get_weight();

					int sekr = SearchEntityKey(p_re->DocId, *static_cast<Entity *>(p_re), &surrogate_scope_id);
					if(sekr > 0) {
						;
					}
					else if(sekr < 0) {
						; // @todo Черт его знает что делать с ошибкой сериализации в частном случае!
					}
					ok = 1;
				}
			}
		}
	}
	CATCHZOK
	CommitTransaction(tra);
	return ok;
}

PPFtsDatabase::Entity::Entity() : Scope(scopeUndef), ObjType(0), ObjId(0)
{
}

PPFtsDatabase::Entity::Entity(const Entity & rS) : Scope(rS.Scope), ObjType(rS.ObjType), ObjId(rS.ObjId), ScopeIdent(rS.ScopeIdent)
{
}

PPFtsDatabase::Entity & FASTCALL PPFtsDatabase::Entity::operator = (const Entity & rS)
{
	Scope = rS.Scope;
	ObjType = rS.ObjType;
	ObjId = rS.ObjId;
	ScopeIdent = rS.ScopeIdent;
	return *this;
}

PPFtsDatabase::Entity & PPFtsDatabase::Entity::Z()
{
	Scope = 0;
	ObjType = 0;
	ObjId = 0;
	ScopeIdent.Z();
	return *this;
}

bool PPFtsDatabase::Entity::IsValid() const
{
	bool   ok = true;
	THROW_PP(oneof2(Scope, scopePPDb, scopeStyloQSvc), PPERR_FTS_INVSCOPEIDENT);
	THROW_PP(ScopeIdent.NotEmpty(), PPERR_FTS_INVSCOPEIDENT);
	THROW_PP(ScopeIdent.Len() <= 256, PPERR_FTS_INVSCOPEIDENT);
	THROW_PP(ScopeIdent.IsLegalUtf8(), PPERR_FTS_INVSCOPEIDENT);
	CATCHZOK
	return ok;
}

bool FASTCALL PPFtsDatabase::Entity::IsEq(const Entity & rS) const
{
	return (Scope == rS.Scope && ObjType == rS.ObjType && ObjId == rS.ObjId && ScopeIdent == rS.ScopeIdent);
}

/*int PPFtsDatabase::Entity::Serialize(int dir, SBuffer & rBuf, SSerializeContext * pSCtx)
{
	int    ok = 1;
	uint32 _prefix = 0;
	THROW_SL(pSCtx->Serialize(dir, _prefix, rBuf));
	THROW_SL(pSCtx->Serialize(dir, Scope, rBuf));
	THROW_SL(pSCtx->Serialize(dir, ScopeIdent, rBuf));
	THROW_SL(pSCtx->Serialize(dir, ObjType, rBuf));
	THROW_SL(pSCtx->Serialize(dir, ObjId, rBuf));
	CATCHZOK
	return ok;
}*/

int PPFtsDatabase::Entity::MakeSurrogateScopeIdent(SBinaryChunk & rSsi) const
{
	int    ok = 1;
	rSsi.Z();
	THROW_SL(rSsi.Cat(&Scope, sizeof(Scope)));
	THROW_SL(rSsi.Cat(ScopeIdent.cptr(), ScopeIdent.Len()));
	CATCHZOK
	return ok;
}

int PPFtsDatabase::Entity::SetSurrogateScopeIdent(const SBinaryChunk & rSsi)
{
	int    ok = 1;
	THROW_PP(rSsi.Len() >= sizeof(Scope), PPERR_FTS_INVSURROGATESCOPEIDENT);
	memcpy(&Scope, rSsi.PtrC(), sizeof(Scope));
	ScopeIdent.Z().CatN(PTRCHRC(rSsi.PtrC()), rSsi.Len()-sizeof(Scope));
	CATCHZOK
	return ok;
}

PPFtsDatabase::Transaction::Transaction(PPFtsDatabase & rMaster) : State(0), R_Master(rMaster),
	P_SKeyT(0), P_SKeyRevT(0), P_SDomainT(0), P_SDomainRevT(0), P_STxn(0)
{
	if(R_Master.P_XDb && R_Master.P_SupplementalDb) {
		P_STxn = new LmdbDatabase::Transaction(*rMaster.P_SupplementalDb, (R_Master.State & stWriting) ? false : true);
		if(P_STxn->GetState() == LmdbDatabase::Transaction::stUndef) {
			State |= stError;
		}
		else {
			P_SKeyT = new LmdbDatabase::Table(*P_STxn, "fts-supplemental-key");
			P_SKeyRevT = new LmdbDatabase::Table(*P_STxn, "fts-supplemental-rev");
			P_SDomainT = new LmdbDatabase::Table(*P_STxn, "fts-supplemental-domain-key");
			P_SDomainRevT = new LmdbDatabase::Table(*P_STxn, "fts-supplemental-domain-rev");
			if(!P_SKeyT->IsValid() || !P_SKeyRevT->IsValid() || !P_SDomainT->IsValid() || !P_SDomainRevT->IsValid()) {
				State |= stError;
			}
		}
		if(!(State & stError)) {
			if(R_Master.State & stWriting) {
				try {
					static_cast<Xapian::WritableDatabase *>(R_Master.P_XDb)->begin_transaction(true);
				} catch(...) { 
					State |= stError;
				}
			}
		}
		if(State & stError) {
			DestroyTables();
			ZDELETE(P_STxn);
		}
	}
	else
		State |= stError;
}

PPFtsDatabase::Transaction::~Transaction()
{
	Abort();
	DestroyTables();
	ZDELETE(P_STxn);
}

void PPFtsDatabase::Transaction::DestroyTables()
{
	ZDELETE(P_SKeyT);
	ZDELETE(P_SKeyRevT);
	ZDELETE(P_SDomainT);
	ZDELETE(P_SDomainRevT);
}
		
bool PPFtsDatabase::Transaction::IsValid() const
{
	return !(State & stError);
}
		
bool PPFtsDatabase::Transaction::Commit()
{
	bool   ok = true;
	if(!(State & (stError|stFinished))) {
		try {
			if(R_Master.State & stWriting) {
				static_cast<Xapian::WritableDatabase *>(R_Master.P_XDb)->commit_transaction();
			}
			if(!P_STxn->Commit()) {
				ok = false;
				State |= stError;
				PPSetError(PPERR_FTS_COMMITTRANSACTIONFAULT);
			}
		} catch(...) {
			ok = false;
			State |= stError;
			PPSetError(PPERR_FTS_COMMITTRANSACTIONFAULT);
		}
	}
	else
		ok = false;
	DestroyTables();
	ZDELETE(P_STxn);
	State |= stFinished;
	return ok;
}

bool PPFtsDatabase::Transaction::Abort()
{
	bool   ok = true;
	if(!(State & (stError|stFinished))) {
		try {
			if(R_Master.State & stWriting) {
				static_cast<Xapian::WritableDatabase *>(R_Master.P_XDb)->cancel_transaction();
			}
			if(!P_STxn->Abort()) {
				ok = false;
				State |= stError;
				PPSetError(PPERR_FTS_ABORTTRANSACTIONFAULT);
			}
		} catch(...) {
			ok = false;
			State |= stError;
			PPSetError(PPERR_FTS_ABORTTRANSACTIONFAULT);
		}
	}
	else
		ok = false;
	DestroyTables();
	ZDELETE(P_STxn);
	State |= stFinished;
	return ok;
}

LmdbDatabase::Table * PPFtsDatabase::Transaction::GetDomainT() { return (State & (stError|stFinished)) ? 0 : P_SDomainT; }
LmdbDatabase::Table * PPFtsDatabase::Transaction::GetDomainRevT() { return (State & (stError|stFinished)) ? 0 : P_SDomainRevT; }
LmdbDatabase::Table * PPFtsDatabase::Transaction::GetKeyT() { return (State & (stError|stFinished)) ? 0 : P_SKeyT; }
LmdbDatabase::Table * PPFtsDatabase::Transaction::GetKeyRevT() { return (State & (stError|stFinished)) ? 0 : P_SKeyRevT; }

int PPFtsDatabase::GetDatabasePath(int dbd, SString & rBuf) const
{
	rBuf.Z();
	int    ok = 1;
	SString base_path;
	PPGetPath(PPPATH_WORKSPACE, base_path);
	base_path.SetLastSlash().Cat("supplementaldata");
	if(dbd == PPFtsDatabase::dbdMain) {
		(rBuf = base_path).SetLastSlash().Cat("fti");
		THROW_SL(createDir(rBuf));
	}
	else if(dbd == PPFtsDatabase::dbdSupplemental) {
		(rBuf = base_path).SetLastSlash().Cat("lmdb");
		THROW_SL(createDir(rBuf));
	}
	else
		ok = 0;
	CATCHZOK
	return ok;
}

PPFtsDatabase::PPFtsDatabase(bool forUpdate) : State(0), P_XDb(0), P_SupplementalDb(0), P_CurrentTra(0)
{
	{
		SString dbpath;
		GetDatabasePath(dbdMain, dbpath);
		if(forUpdate) {
			P_XDb = new Xapian::WritableDatabase(dbpath.cptr(), Xapian::DB_CREATE_OR_OPEN, 0);
			State |= stWriting;
		}
		else {
			P_XDb = new Xapian::Database(dbpath.cptr(), Xapian::DB_OPEN);
		}
		// @todo process exception if any
	}
	{
		SString suppemental_db_path;
		GetDatabasePath(dbdSupplemental, suppemental_db_path);
		P_SupplementalDb = LmdbDatabase::GetInstance(suppemental_db_path, 0, 0);
		if(!P_SupplementalDb)
			State |= stError;
	}
}
	
PPFtsDatabase::~PPFtsDatabase()
{
	delete P_CurrentTra;
	delete P_XDb;
	P_SupplementalDb = 0; // !Мы не владеем экземпляром, на который указывает P_SupplementalDb
}

SHandle PPFtsDatabase::BeginTransaction()
{
	if(P_CurrentTra) {
		return SHandle(0);
	}
	else {
		P_CurrentTra = new Transaction(*this);
		if(P_CurrentTra && !P_CurrentTra->IsValid()) {
			PPSetError(PPERR_FTS_BEGINTRANSACTIONFAULT);
			ZDELETE(P_CurrentTra);
		}
		return SHandle(P_CurrentTra);
	}
}

int PPFtsDatabase::CommitTransaction(SHandle tra)
{
	int    ok = 1;
	if(P_CurrentTra && P_CurrentTra == tra)  {
		ok = P_CurrentTra->Commit();
		ZDELETE(P_CurrentTra);
	}
	return ok;
}

int PPFtsDatabase::AbortTransaction(SHandle tra)
{
	int    ok = 1;
	if(P_CurrentTra && P_CurrentTra == tra)  {
		ok = P_CurrentTra->Abort();
		ZDELETE(P_CurrentTra);
	}
	return ok;
}

int PPFtsDatabase::SearchEntityKey(uint64 key, Entity & rEnt, uint64 * pSurrogateScopeIdent)
{
	int    ok = 1;
	uint64 ssi_db_id = 0;
	THROW(P_CurrentTra);
	{
		SSerializeContext sctx;
		SBaseBuffer val_buf;
		LmdbDatabase::Table * p_domain_t = P_CurrentTra->GetDomainT();
		LmdbDatabase::Table * p_domain_rev_t = P_CurrentTra->GetDomainRevT();
		LmdbDatabase::Table * p_key_t = P_CurrentTra->GetKeyT();
		LmdbDatabase::Table * p_key_rev_t = P_CurrentTra->GetKeyRevT();
		THROW(p_domain_t);
		THROW(p_domain_rev_t);
		THROW(p_key_t);
		THROW(p_key_rev_t);
		if(p_key_rev_t->Get(&key, sizeof(key), val_buf) > 0) {
			SBuffer entity_buf;
			entity_buf.Z().Write(val_buf.P_Buf, val_buf.Size);
			{
				uint32 _prefix = 0;
				THROW_SL(sctx.Serialize(-1, _prefix, entity_buf));
				THROW(_prefix == 0);
				THROW_SL(sctx.Serialize(-1, ssi_db_id, entity_buf));
				THROW_SL(sctx.Serialize(-1, rEnt.ObjType, entity_buf));
				THROW_SL(sctx.Serialize(-1, rEnt.ObjId, entity_buf));
			}
			THROW(ssi_db_id > 0);
			{
				SBinaryChunk ssi_buf;
				int r = p_domain_rev_t->Get(&ssi_db_id, sizeof(ssi_db_id), ssi_buf);
				THROW(r > 0); // Если доменный идентификатор не найден, то это - сбойная ситуация.
				THROW(rEnt.SetSurrogateScopeIdent(ssi_buf));
			}
		}
		else
			ok = -1;
	}
	CATCHZOK
	ASSIGN_PTR(pSurrogateScopeIdent, ssi_db_id);
	return ok;
}

int PPFtsDatabase::GetEntityKey(Entity & rEnt, uint64 * pSurrogateScopeIdent, SBuffer & rEntityBuf, uint64 * pKey)
{
	rEntityBuf.Z();
	int    ok = 1;
	const  Entity org_ent(rEnt); // @proof-of-immutability
	uint64 ssi_db_id = 0;
	uint64 key = 0;
	SSerializeContext sctx;
	THROW_PP(P_CurrentTra && P_CurrentTra->IsValid(), PPERR_FTS_INVALIDTRANSACTION);
	{
		LmdbDatabase::Table * p_domain_t = P_CurrentTra->GetDomainT();
		LmdbDatabase::Table * p_domain_rev_t = P_CurrentTra->GetDomainRevT();
		LmdbDatabase::Table * p_key_t = P_CurrentTra->GetKeyT();
		LmdbDatabase::Table * p_key_rev_t = P_CurrentTra->GetKeyRevT();
		SBinaryChunk ssi; // surrogate scope ident
		THROW(p_domain_t);
		THROW(p_domain_rev_t);
		THROW(p_key_t);
		THROW(p_key_rev_t);
		THROW(rEnt.MakeSurrogateScopeIdent(ssi));
		{
			SBaseBuffer _id_buf;
			int gr = p_domain_t->Get(ssi.PtrC(), ssi.Len(), _id_buf);
			THROW(gr);
			if(gr > 0) {
				assert(_id_buf.P_Buf);
				assert(_id_buf.Size > 0);
				THROW(_id_buf.P_Buf && _id_buf.Size == sizeof(uint64));
				ssi_db_id = *reinterpret_cast<const uint64 *>(_id_buf.P_Buf);
			}
			else {
				SBaseBuffer _try_id_buf;
				LmdbDatabase::Stat tstat;
				THROW(p_domain_t->GetStat(tstat));
				ssi_db_id = tstat.EntryCount;
				int gr2 = 0;
				do {
					ssi_db_id++;
					gr2 = p_domain_rev_t->Get(&ssi_db_id, sizeof(ssi_db_id), _try_id_buf);
					THROW(gr2);
				} while(gr2 > 0);
				THROW(p_domain_t->Put(ssi.PtrC(), ssi.Len(), &ssi_db_id, sizeof(ssi_db_id), 0));
				THROW(p_domain_rev_t->Put(&ssi_db_id, sizeof(ssi_db_id), ssi.PtrC(), ssi.Len(), 0));
			}
			THROW(ssi_db_id);
		}
		//THROW_SL(rEnt.Serialize(+1, entity_buf, &sctx));
		{
			uint32 _prefix = 0;
			THROW_SL(sctx.Serialize(+1, _prefix, rEntityBuf));
			THROW_SL(sctx.Serialize(+1, ssi_db_id, rEntityBuf));
			THROW_SL(sctx.Serialize(+1, rEnt.ObjType, rEntityBuf));
			THROW_SL(sctx.Serialize(+1, rEnt.ObjId, rEntityBuf));
		}
		{
			SBaseBuffer _id_buf;
			int gr = p_key_t->Get(rEntityBuf.constptr(), rEntityBuf.GetAvailableSize(), _id_buf);
			if(gr > 0) {
				assert(_id_buf.P_Buf);
				assert(_id_buf.Size > 0);
				THROW(_id_buf.P_Buf && _id_buf.Size == sizeof(uint64));
				key = *reinterpret_cast<const uint64 *>(_id_buf.P_Buf);
			}
			else
				ok = -1;
		}
	}
	CATCHZOK
	assert(rEnt == org_ent); // @proof-of-immutability
	assert(ok == 0 || ssi_db_id > 0);
	assert(ok <= 0 || key > 0);
	assert(ok > 0  || key == 0);
	ASSIGN_PTR(pSurrogateScopeIdent, ssi_db_id);
	ASSIGN_PTR(pKey, key);
	return ok;
}

int PPFtsDatabase::StoreEntityKey(uint64 surrogateScopeIdent, const SBuffer & rEntityBuf, uint64 key)
{
	int    ok = 1;
	assert(surrogateScopeIdent > 0);
	assert(key > 0);
	assert(rEntityBuf.GetAvailableSize() > 0);
	THROW(surrogateScopeIdent > 0);
	THROW(key > 0);
	THROW(rEntityBuf.GetAvailableSize() > 0);
	THROW(P_CurrentTra);
	{
		LmdbDatabase::Table * p_key_t = P_CurrentTra->GetKeyT();
		LmdbDatabase::Table * p_key_rev_t = P_CurrentTra->GetKeyRevT();
		THROW(p_key_t);
		THROW(p_key_rev_t);
		THROW(p_key_t->Put(rEntityBuf.constptr(), rEntityBuf.GetAvailableSize(), &key, sizeof(key), 0));
		THROW(p_key_rev_t->Put(&key, sizeof(key), rEntityBuf.constptr(), rEntityBuf.GetAvailableSize(), 0));
	}
	CATCHZOK
	return ok;
}

uint64 PPFtsDatabase::PutEntity(SHandle tra, Entity & rEnt, StringSet & rSsUtf8)
{
	uint64  result = 0;
	SString temp_buf;
	THROW_PP(State & stWriting, PPERR_FTS_DBINRDONLYMODE);
	THROW_PP(!(State & stError), PPERR_FTS_DBINERRORSTATE);
	THROW(P_XDb);
	THROW_PP(P_CurrentTra && P_CurrentTra == tra && P_CurrentTra->IsValid(), PPERR_FTS_INVALIDTRANSACTION);
	{
		SSerializeContext sctx;
		Xapian::Document doc;
		std::string temp_stds;
		SBuffer entity_buf;
		uint64   doc_id = 0;
		LmdbDatabase::Table * p_domain_t = P_CurrentTra->GetDomainT();
		LmdbDatabase::Table * p_domain_rev_t = P_CurrentTra->GetDomainRevT();
		LmdbDatabase::Table * p_key_t = P_CurrentTra->GetKeyT();
		LmdbDatabase::Table * p_key_rev_t = P_CurrentTra->GetKeyRevT();
		SBinaryChunk ssi; // surrogate scope ident
		uint64   ssi_db_id = 0;
		const int gekr = GetEntityKey(rEnt, &ssi_db_id, entity_buf, &doc_id);
		THROW(gekr);
		try {
			for(uint sp = 0; rSsUtf8.get(&sp, temp_buf);) {
				THROW_SL(temp_buf.IsLegalUtf8());
				temp_stds.clear();
				temp_stds.append(temp_buf, temp_buf.Len());
				doc.add_term(temp_stds);
				//
				//doc.add_posting();
			}
			{
				Xapian::WritableDatabase * p_db = static_cast<Xapian::WritableDatabase *>(P_XDb);
				if(doc_id > 0) {
					p_db->replace_document(doc_id, doc);
					result = doc_id;
				}
				else {
					doc_id = p_db->add_document(doc);
					if(doc_id) {
						THROW(StoreEntityKey(ssi_db_id, entity_buf, doc_id));
						result = doc_id;
					}
				}
			}
		} catch(...) {
			result = PPSetError(PPERR_FTS_PUTDOCFAULT);
		}
	}
	CATCH
		result = 0;
	ENDCATCH
	return result;
}

static int Test_Fts2()
{
	int    ok = 1;
	uint   search_err_count = 0;
	uint   last_word_id = 0;
	//SymbHashTable word_list(SKILOBYTE(1024), 0);
	LAssocArray word_to_doc_assoc;
	DbProvider * p_dict = CurDict;
	THROW(p_dict);
	{
		PPFtsDatabase db(true);
		SHandle tra;
		PPObjGoods goods_obj;
		PPViewGoods v_goods;
		GoodsFilt f_goods;
		S_GUID db_uuid;
		SString db_symb;
		SString text_buf;
		SString text_resource_ident;
		p_dict->GetDbUUID(&db_uuid);
		p_dict->GetDbSymb(db_symb);

		PPTextAnalyzer text_analyzer2;
		PPTextAnalyzer::Item text_analyzer_item;
		STokenizer::Param tparam;
		tparam.Cp = cpUTF8;
		//tparam.Flags |= STokenizer::fDivAlNum;
		tparam.Delim = " \t\n\r(){}[]<>,.:;-\\/&$#@!?*^\"+=%\xA0";
		tparam.Flags |= (STokenizer::fDivAlNum|STokenizer::fEachDelim);
		text_analyzer2.SetParam(&tparam);
		PPWait(1);
		if(v_goods.Init_(&f_goods)) {
			uint   total_count = 0;
			GoodsViewItem vitem;
			BarcodeArray bc_list;
			StringSet doc_tokens;
			PPFtsDatabase::Entity entity;
			LongArray current_word_id_list;
			entity.Scope = PPFtsDatabase::scopePPDb;
			if(!!db_uuid)
				entity.ScopeIdent.Z().Cat(db_uuid);
			else
				entity.ScopeIdent = db_symb;
			entity.ObjType = PPOBJ_GOODS;
			tra = db.BeginTransaction();
			THROW(tra);
			for(v_goods.InitIteration(PPViewGoods::OrdByDefault); v_goods.NextIteration(&vitem) > 0;) {
				entity.ObjId = vitem.ID;
				current_word_id_list.Z();
				text_analyzer2.Reset(STokenizer::coClearSymbTab);
				{
					text_resource_ident.Z().CatChar('#').CatLongZ(vitem.ID, 8);
					(text_buf = vitem.Name).Transf(CTRANSF_INNER_TO_UTF8).Utf8ToLower();
					if(text_buf.C(0) == '\"' && text_buf.Last() == '\"') {
						text_buf.TrimRightChr('\"');
						text_buf.ShiftLeftChr('\"');
						text_buf.ReplaceStr("\"\"", "\"", 0);
					}
					{
						uint   idx_first = 0;
						uint   idx_count = 0;
						PROFILE(THROW_SL(text_analyzer2.Write(text_resource_ident, 0, text_buf, text_buf.Len()+1)));
						PROFILE(THROW_SL(text_analyzer2.Run(&idx_first, &idx_count)));
						//
						doc_tokens.Z();
						for(uint j = 0; j < idx_count; j++) {
							if(text_analyzer2.Get(idx_first+j, text_analyzer_item)) {
								if(text_analyzer_item.Token == STokenizer::tokWord) {
									doc_tokens.add(text_analyzer_item.Text);
									//
									/*
									uint   word_id = 0;
									if(word_list.Search(text_analyzer_item.Text, &word_id, 0)) {
										current_word_id_list.add(static_cast<long>(word_id));
									}
									else {
										word_id = ++last_word_id;
										word_list.Add(text_analyzer_item.Text, word_id);
										current_word_id_list.add(static_cast<long>(word_id));
									}*/
								}
							}
						}
						{
							goods_obj.P_Tbl->ReadBarcodes(vitem.ID, bc_list);
							for(uint bcidx = 0; bcidx < bc_list.getCount(); bcidx++) {
								text_buf = bc_list.at(bcidx).Code;
								text_buf.Transf(CTRANSF_INNER_TO_UTF8);
								doc_tokens.add(text_buf);
								//
								/*
								uint   word_id = 0;
								if(word_list.Search(text_buf, &word_id, 0)) {
									current_word_id_list.add(static_cast<long>(word_id));
								}
								else {
									word_id = ++last_word_id;
									word_list.Add(text_buf, word_id);
									current_word_id_list.add(static_cast<long>(word_id));
								}*/
							}
						}
						{
							uint64 result_doc_id = db.PutEntity(tra, entity, doc_tokens);
							THROW(result_doc_id);
							//
							/*{
								for(uint cwidx = 0; cwidx < current_word_id_list.getCount(); cwidx++) {
									word_to_doc_assoc.Add(current_word_id_list.get(cwidx), static_cast<long>(result_doc_id));
								}
							}*/
							total_count++;
							if(total_count > 0 && (total_count % 1000) == 0) {
								THROW(db.CommitTransaction(tra));
								tra = db.BeginTransaction();
								THROW(tra);
							}
							// 
							//if(v_goods.P_Tbl->GetBarcode)
							//f_out_name.WriteLine(msg_buf.CR());
						}
						PPWaitPercent(v_goods.GetCounter());
					}
				}
			}
			THROW(db.CommitTransaction(tra));
		}
	}
#if 0 // {
	if(0) {
		//
		// Теперь проверяем поиск
		//
		PPFtsDatabase db(false);
		word_to_doc_assoc.SortByKeyVal();
		SymbHashTable::Iter iter;
		SymbHashTable::Stat hts;
		word_list.CalcStat(hts);
		uint   iter_no = 0;
		if(word_list.InitIteration(&iter)) {
			uint word_id = 0;
			SString word_buf;
			LongArray expected_doc_id_list;
			while(word_list.NextIteration(&iter, &word_id, 0, &word_buf)) {
				bool expected_doc_presents = false;
				TSCollection <PPFtsDatabase::SearchResultEntry> result_list;
				expected_doc_id_list.Z();
				word_to_doc_assoc.GetListByKey(word_id, expected_doc_id_list);
				int sr = db.Search(word_buf, result_list);
				if(sr > 0) {
					assert(result_list.getCount());
					for(uint rlidx = 0; rlidx < result_list.getCount(); rlidx++) {
						const PPFtsDatabase::SearchResultEntry * p_re = result_list.at(rlidx);
						if(p_re) {
							const long found_doc_id = static_cast<long>(p_re->DocId);
							if(expected_doc_id_list.lsearch(found_doc_id))
								expected_doc_presents = true;
						}
					}
				}
				if(!expected_doc_presents)
					search_err_count++;
				//
				iter_no++;
				PPWaitPercent(iter_no, hts.NumEntries);
			}
		}
	}
#endif // } 0
	CATCH
		PPLogMessage(PPFILNAM_ERR_LOG, 0, LOGMSGF_LASTERR|LOGMSGF_TIME|LOGMSGF_DBINFO);
		ok = 0;
	ENDCATCH
	PPWait(0);
	return ok;
}

int  Test_Fts()
{
	int    ok = 1;
	THROW(Test_Fts2());
	if(0) {
		const char * pp_text_file_names[] = { "rustext.txt", "phrases-ru-1251.txt", "person.txt", "address.txt" };
		SString base_path;
		SString file_path;
		SString temp_buf;
		STempBuffer raw_input_buf(SKILOBYTE(512));
		SStringU input_text_u;
		SString input_text_utf8;
		UErrorCode icu_status = U_ZERO_ERROR;
		{
			//
			// Пробуем работать с базой данных LMDB
			//
			SString lmdb_path;
			PPGetPath(PPPATH_WORKSPACE, lmdb_path);
			lmdb_path.SetLastSlash().Cat("lmdb");
			createDir(lmdb_path);
			LmdbDatabase * p_db = LmdbDatabase::GetInstance(lmdb_path, 0, 0);
			if(p_db) {
				LmdbDatabase * p_db2 = LmdbDatabase::GetInstance(lmdb_path, 0, 0);
				assert(p_db2 == p_db);
				{
					uint put_err = 0;
					uint get_err = 0;
					uint get_cmp_err = 0;
					UuidArray uuidlist;
					{
						LmdbDatabase::Transaction txn(*p_db, false);
						LmdbDatabase::Table tbl(txn, "fts-supplement");
						if(tbl.IsValid()) {
							for(uint i = 0; i < 1000000; i++) {
								S_GUID uuid(SCtrGenerate_);
								temp_buf.Z().Cat(uuid);
								uuidlist.insert(&uuid);
								if(!tbl.Put(&uuid, sizeof(uuid), temp_buf.cptr(), temp_buf.Len(), 0))
									put_err++;
							}
							txn.Commit();
						}
					}
					{
						SString result_val_text;
						LmdbDatabase::Transaction txn(*p_db, true);
						LmdbDatabase::Table tbl(txn, "fts-supplement");
						if(tbl.IsValid()) {
							uuidlist.shuffle();
							for(uint i = 0; i < uuidlist.getCount(); i++) {
								S_GUID uuid = uuidlist.at(i);
								temp_buf.Z().Cat(uuid);
								SBaseBuffer val_buf;
								int r = tbl.Get(&uuid, sizeof(uuid), val_buf);
								if(r > 0) {
									assert(val_buf.P_Buf != 0);
									result_val_text.Z().CatN(val_buf.P_Buf, val_buf.Size);
									if(temp_buf != result_val_text) {
										get_cmp_err++;
									}
								}
								else {
									get_err++;
								}
							}
						}
					}
				}
			}
		}
		{
			PPGetPath(PPPATH_BIN, temp_buf);
			u_setDataDirectory(temp_buf);
			u_init(&icu_status);
		}
		Locale icu_locale("ru", "RU");
		//SLS.QueryPath("testroot", base_path);
		base_path = "\\papyrus\\src\\pptest";
		base_path.SetLastSlash().Cat("data").SetLastSlash();
		{
			PPGetFilePath(PPPATH_OUT, "fts-token-list.txt", temp_buf);
			SFile f_tokens(temp_buf, SFile::mWrite);
			Xapian::WritableDatabase db("xapian-test-db", Xapian::DB_CREATE_OR_OPEN, 0);
			static const wchar_t p_puncts[] = { L'.', L',', L';', L':', L' ', L'\t', L'\n', L'-', L'(', L')', L'[', L']', L'{', L'}', L'!', L'\"', L'+', L'/', L'=', L'?', L'\\', L'|', L'№' };
			for(uint i = 0; i < SIZEOFARRAY(pp_text_file_names); i++) {
				(file_path = base_path).Cat(pp_text_file_names[i]);
				SFile f_in(file_path, SFile::mRead);
				if(f_in.IsValid()) {
					int64 file_size = 0;
					if(f_in.CalcSize(&file_size)) {
						assert(file_size >= 0);
						if(file_size == 0) {
							; // file is empty
						}
						else if(file_size > SMEGABYTE(128)) {
							; // file too big
						} 
						else {
							size_t needed_size = static_cast<size_t>(file_size + 512); // 512 - insuring
							size_t actual_size;
							Xapian::Document doc;
							db.begin_transaction(true);
							THROW_SL(raw_input_buf.AllocIncr(needed_size));
							if(f_in.Read(raw_input_buf, static_cast<size_t>(file_size), &actual_size)) {
								BreakIterator * p_bi = BreakIterator::createWordInstance(icu_locale, icu_status);
								if(U_SUCCESS(icu_status)) {
									f_tokens.WriteLine((temp_buf = file_path).CR()); // @debug
									//input_text_utf8.Z().CatN(raw_input_buf, actual_size).Transf(CTRANSF_OUTER_TO_UTF8);
									input_text_u.CopyFromMb(cp1251, raw_input_buf, actual_size);
									UnicodeString us(input_text_u);
									//input_text.Z().CopyFromMb(cp1251, raw_input_buf, actual_size);
									p_bi->setText(us);
									int32 prev_boundary = 0;
									int32 p = p_bi->first();
									if(p != BreakIterator::DONE) do {
										//
										auto word = us.tempSubStringBetween(prev_boundary, p);
										char buffer[16384];
										bool skip = false;
										word.trim();
										if(word.length() == 0)
											skip = true;
										else if(word.length() == 1) {
											for(uint pidx = 0; !skip && pidx < SIZEOFARRAY(p_puncts); pidx++) {
												if(word.charAt(0) == p_puncts[pidx])
													skip = true;
											}
										}
										if(!skip) {
											word.toLower();
											word.toUTF8(CheckedArrayByteSink(buffer, sizeof(buffer)));
											doc.add_term(buffer);
											f_tokens.WriteLine((temp_buf = buffer).CR());
											//words.emplace_back(QString::fromUtf8(buffer));
											//
										}
										prev_boundary = p;
										p = p_bi->next();
									} while(p != BreakIterator::DONE);
								}
							}
							Xapian::docid did = db.add_document(doc);
							db.commit_transaction();
						}
					}
				}
			}
		}
		{
			bool debug_mark = false;
			SStringU qb_u;
			std::string found_doc_text;
			Xapian::Database db("xapian-test-db", Xapian::DB_OPEN);
			Xapian::Enquire enq(db);
			Xapian::QueryParser qp;
			qp.set_database(db);
			temp_buf = "автоматически";
			qb_u.CopyFromUtf8(temp_buf);
			Xapian::Query q = qp.parse_query(temp_buf.cptr());
			std::string qdescr = q.get_description();
			//
			enq.set_query(q);
			Xapian::MSet ms = enq.get_mset(0, 10);
			for(Xapian::MSetIterator i = ms.begin(); i != ms.end(); ++i) {
				Xapian::doccount rank = i.get_rank();
				double wt = i.get_weight();
				Xapian::docid did = *i;
				found_doc_text = i.get_document().get_data();
				//cout << i.get_rank() + 1 << ": " << i.get_weight() << " docid=" << *i << " [" << i.get_document().get_data() << "]\n\n";			
			}
			debug_mark = true;
		}
	}
	CATCH
		ok = 0;
	ENDCATCH
	return ok;
}

#endif // } 0
