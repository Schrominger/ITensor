//
// Distributed under the ITensor Library License, Version 1.2
//    (See accompanying LICENSE file.)
//
#ifndef __ITENSOR_DOTASK_H
#define __ITENSOR_DOTASK_H

#include <cassert>
#include "itensor/itdata/itdata.h"
#include "itensor/util/print.h"
#include "itensor/util/stdx.h"

namespace itensor {

///////////////////

namespace detail {

template<typename F,typename Ret>
struct ApplyFunc
    { 
    using function_type = F;
    F& f;
    Ret r;
    ApplyFunc(F&& f_) : f(f_) { }
    template<typename S>
    void
    operator()(S& s) 
        { 
        r = f(s); 
        }
    operator Ret() const { return std::move(r); }
    };

template<typename F>
struct ApplyFunc<F,void>
    { 
    using function_type = F;
    F& f;
    ApplyFunc(F&& f_) : f(f_) { }
    template<typename S>
    void
    operator()(S& s) { f(s); }
    };

template<typename F, typename R, typename Storage>
void
applyFunc_impl(stdx::choice<3>, ApplyFunc<F,R>& A, const Storage& s, ManageStore& m)
    {
    throw ITError("applyFunc: function object has no operator() method for storage type");
    }

template<typename F, typename R, typename Storage>
auto
applyFunc_impl(stdx::choice<2>, ApplyFunc<F,R>& A, const Storage& s, ManageStore& m)
    -> decltype(A.f(s), void())
    {
    Storage& ncs = m.modifyData();
    A(ncs);
    }

template<typename F, typename R, typename Storage>
auto
applyFunc_impl(stdx::choice<1>, ApplyFunc<F,R>& A, const Storage& s, ManageStore& m) 
    -> decltype(A.f(s), void())
    {
    A(s);
    }

} //namespace detail

template<typename F, typename R, typename Storage>
void
doTask(detail::ApplyFunc<F,R>& A, const Storage& s, ManageStore& m) 
    { 
    detail::applyFunc_impl(stdx::select_overload{},A,s,m);
    }


namespace detail {

//OneArg and TwoArgs are "policy classes" 
//for customizing implementation of RegisterTask
template<class PType>
struct OneArg
    {
    using ptype = PType;

    //template<typename RT, typename Task, typename D, typename Return>
    //void
    //call(RT& rt, Task& t, const D& d, ManageStore& m, Return& ret);

    template<typename RT, typename Task, typename D, typename Return>
    void
    call(RT&, Task& t, D& d, ManageStore& m, Return& ret);
    };

template<class PType1, class PType2>
struct TwoArgs
    {
    using ptype1 = PType1;
    using ptype2 = PType2;

    //template<typename RT, typename Task, typename D, typename Return>
    //void
    //call(RT& rt, Task& t, const D& d, ManageStore& m, Return& ret);

    template<typename RT, typename Task, typename D, typename Return>
    void
    call(RT& rt, Task& t, D& d, ManageStore& m, Return& ret);
    };

template<typename Derived, typename TList>
struct FuncT : FuncT<Derived,popFront<TList>>
    {
    using T = frontType<TList>;
    using FuncT<Derived,popFront<TList>>::applyTo;

    void
    applyTo(const T& t) final
        {
        auto* pd = static_cast<Derived*>(this);
        pd->applyToImpl(t);
        }

    void
    applyTo(T& t) final
        {
        auto* pd = static_cast<Derived*>(this);
        pd->applyToImpl(t);
        }
    };
template<typename Derived>
struct FuncT<Derived,TypeList<>> : FuncBase
    { };

template <typename NArgs, typename Task, typename Return>
class RegisterTask;

template<typename NArgs, typename Task, typename Return>
Task
getReturnHelperImpl(stdx::choice<2>, RegisterTask<NArgs,Task,Return> & R)
    {
    return std::move(R.task_);
    }
template<typename NArgs, typename Task, typename Return>
auto
getReturnHelperImpl(stdx::choice<1>, RegisterTask<NArgs,Task,Return> & R)
    -> stdx::enable_if_t<std::is_same<typename RegisterTask<NArgs,Task,Return>::return_type,Return>::value,Return>
    {
    return std::move(R.ret_);
    }
template<typename NArgs, typename Task, typename Return>
auto
getReturnHelper(RegisterTask<NArgs,Task,Return> & R)
    -> decltype(getReturnHelperImpl(stdx::select_overload{},R))
    {
    return getReturnHelperImpl(stdx::select_overload{},R);
    }


template <typename NArgs, typename Task, typename Return>
class RegisterTask : public FuncT<RegisterTask<NArgs,Task,Return>,StorageTypes>
    {
    public:
    using task_type = stdx::remove_reference_t<Task>;
    using return_type = stdx::conditional_t<std::is_same<Return,NoneType>::value,
                                           task_type,
                                           Return>;
    task_type task_;
    ManageStore m_;
    Return ret_;

    RegisterTask(const task_type& t)
      : task_(t)
        { }

    RegisterTask(task_type&& t)
      : task_(std::move(t))
        { }

    RegisterTask(task_type&& t,
                 ManageStore&& m)
      : task_(std::move(t)),
        m_(std::move(m))
        { }

    RegisterTask(const task_type& t,
                 ManageStore&& m)
      : task_(t),
        m_(std::move(m))
        { }

    virtual ~RegisterTask() { }

    return_type
    getReturn() { return getReturnHelper(*this); }

    template<typename D>
    void
    applyToImpl(D& d);
    };

template <class RT, typename Task, typename D1, typename Return, class PType1, class PType2>
class CallWrap : public FuncT<CallWrap<RT,Task,D1,Return,PType1,PType2>,StorageTypes>
    {
    RT& rt_;
    Task& t_;
    D1& d1_;
    ManageStore& m_;
    Return& ret_;
    public:

    using ptype1 = PType1;
    using ptype2 = PType2;

    CallWrap(RT& rt, Task& t, D1& arg1, ManageStore& m, Return& ret) 
        : rt_(rt), t_(t), d1_(arg1), m_(m), ret_(ret) { }

    template<typename D2>
    void
    applyToImpl(D2& d2);
    
    };

//
// Implementations
//

template<typename Ret>
using RetOrNone = stdx::conditional_t<std::is_void<Ret>::value,NoneType,Ret>;

template<typename Task, typename Storage>
NoneType
testRetImpl(stdx::choice<3>, Task& t, Storage& s, ManageStore& m)
    {
    return NoneType{};
    }
template<typename Task, typename Storage>
auto 
testRetImpl(stdx::choice<2>, Task& t, Storage& s, ManageStore& m)
    -> RetOrNone<decltype(doTask(t,s))>
    {
    using Ret = RetOrNone<decltype(doTask(t,s))>;
    return Ret{};
    }
template<typename Task, typename Storage>
auto 
testRetImpl(stdx::choice<1>, Task& t, Storage& s, ManageStore& m)
    -> RetOrNone<decltype(doTask(t,s,m))>
    {
    using Ret = RetOrNone<decltype(doTask(t,s,m))>;
    return Ret{};
    }
template<typename Task, typename Storage>
struct TestRet
    {
    Task& t;
    Storage& s;
    ManageStore& m;
    TestRet(Task& t_, Storage& s_, ManageStore& m_)
        : t(t_), s(s_), m(m_) 
        { }
    auto
    operator()() -> decltype(testRetImpl(stdx::select_overload{},t,s,m))
        {
        return testRetImpl(stdx::select_overload{},t,s,m);
        }
    };

/////////////

//If ActualRet!=void this gets called
template<typename Ret, typename ActualRet>
class FixRet
    {
    Ret& ret_;
    public:

    FixRet(Ret& ret) : ret_(ret) { }

    template<typename... VArgs>
    void
    operator()(VArgs&&... vargs)
        {
        ret_ = doTask(std::forward<VArgs>(vargs)...);
        }
    };
//If ActualRet==void this gets called
template<typename Ret>
class FixRet<Ret,void>
    {
    public:

    FixRet(Ret& ret) { }

    template<typename... VArgs>
    void
    operator()(VArgs&&... vargs)
        {
        doTask(std::forward<VArgs>(vargs)...);
        }
    };

/////////////

template <typename Task, typename Storage, typename Ret>
void
callDoTask_Impl(stdx::choice<3>, Task& t, Storage& s, ManageStore& m, Ret& ret)
    {
    static_assert(containsType<StorageTypes,stdx::decay_t<Storage>>{},"Data type not in list of registered storage types");
    throw ITError("1 parameter doTask not defined for specified task or data type [1]");
    }
template <typename Task, typename Storage, typename Ret>
auto 
callDoTask_Impl(stdx::choice<2>, Task& t, Storage& s, ManageStore& m, Ret& ret)
    -> stdx::if_compiles_return<void,decltype(doTask(t,s))>
    {
    static_assert(containsType<StorageTypes,stdx::decay_t<Storage>>{},"Data type not in list of registered storage types");
    FixRet<Ret,decltype(doTask(t,s))>{ret}(t,s);
    }
template <typename Task, typename Storage, typename Ret>
auto 
callDoTask_Impl(stdx::choice<1>, Task& t, Storage& s, ManageStore& m, Ret& ret)
    -> stdx::if_compiles_return<void,decltype(doTask(t,s,m))>
    {
    static_assert(containsType<StorageTypes,stdx::decay_t<Storage>>{},"Data type not in list of registered storage types");
    FixRet<Ret,decltype(doTask(t,s,m))>{ret}(t,s,m);
    }
template<typename Task, typename Storage, typename Return>
void
callDoTask(Task& t, Storage& s, ManageStore& m, Return& ret)
    {
    static_assert(containsType<StorageTypes,stdx::decay_t<Storage>>{},"Data type not in list of registered storage types");
    callDoTask_Impl<Task,Storage,Return>(stdx::select_overload{},t,s,m,ret);
    }

/////////////////////////////////////////////////////

template <typename Ret, typename Task, typename D1, typename D2>
void
callDoTask_Impl(stdx::choice<3>, Task& t, D1& d1, const D2& d2, ManageStore& m, Ret& ret)
    {
    throw ITError("2 parameter doTask not defined for specified task or data type [2]");
    }
template <typename Ret, typename Task, typename D1, typename D2>
auto 
callDoTask_Impl(stdx::choice<2>, Task& t, D1& d1, const D2& d2, ManageStore& m, Ret& ret)
    -> stdx::if_compiles_return<void,decltype(doTask(t,d1,d2))>
    {
    FixRet<Ret,decltype(doTask(t,d1,d2))>{ret}(t,d1,d2);
    }
template <typename Ret, typename Task, typename D1, typename D2>
auto 
callDoTask_Impl(stdx::choice<1>, Task& t, D1& d1, const D2& d2, ManageStore& m, Ret& ret)
    -> stdx::if_compiles_return<void,decltype(doTask(t,d1,d2,m))>
    {
    FixRet<Ret,decltype(doTask(t,d1,d2,m))>{ret}(t,d1,d2,m);
    }
template<typename Ret, typename Task, typename D1, typename D2>
void
callDoTask(Task& t, D1& d1, const D2& d2, ManageStore& m, Ret& ret)
    {
    callDoTask_Impl<Ret,Task,D1,D2>(stdx::select_overload{},t,d1,d2,m,ret);
    }

/////////////////////

template<typename Task, typename D>
std::false_type
testDTImpl(stdx::choice<3>, Task& t, D& d, ManageStore& m)
    {
    return std::false_type{};
    }
template<typename Task, typename D>
auto 
testDTImpl(stdx::choice<2>, Task& t, D& d, ManageStore& m)
    -> stdx::if_compiles_return<std::true_type,decltype(doTask(t,d))>
    {
    return std::true_type{};
    }
template<typename Task, typename D>
auto 
testDTImpl(stdx::choice<1>, Task& t, D& d, ManageStore& m)
    -> stdx::if_compiles_return<std::true_type,decltype(doTask(t,d,m))>
    {
    return std::true_type{};
    }
template<typename Task, typename Storage>
struct HasDTHelper
    {
    Task* t;
    Storage* s;
    ManageStore* m;
    auto operator()() -> decltype(testDTImpl(stdx::select_overload{},*t,*s,*m))
        { return testDTImpl(stdx::select_overload{},*t,*s,*m); }
    };
template<typename Task, typename Storage>
struct HasConstDoTask
    {
    using ResultType = stdx::result_of_t<HasDTHelper<Task,const Storage>()>;
    bool constexpr static
    result() { return ResultType{}; }
    };
template<typename Task, typename Storage>
struct HasNonConstDoTask
    {
    using ResultType = stdx::result_of_t<HasDTHelper<Task,stdx::remove_const_t<Storage>>()>;
    using CResultType = stdx::result_of_t<HasDTHelper<Task,const Storage>()>;
    bool constexpr static
    result() { return ResultType{} && (not CResultType{}); }
    };
template<typename Task, typename Storage>
struct HasDoTask
    {
    using CResultType = stdx::result_of_t<HasDTHelper<Task,const Storage>()>;
    using NCResultType = stdx::result_of_t<HasDTHelper<Task,stdx::remove_const_t<Storage>>()>;
    using ResultType = stdx::conditional_t<CResultType::value,
                                          CResultType,
                                          NCResultType>;
    bool constexpr static
    result() { return ResultType{}; }
    };

/////////////////////

template<typename Task, typename D1, typename D2>
std::false_type
testDTImpl(stdx::choice<3>, Task& t, D1& d1, const D2& d2, ManageStore& m)
    {
    return std::false_type{};
    }
template<typename Task, typename D1, typename D2>
auto 
testDTImpl(stdx::choice<2>, Task& t, D1& d1, const D2& d2, ManageStore& m)
    -> stdx::if_compiles_return<std::true_type,decltype(doTask(t,d1,d2))>
    {
    return std::true_type{};
    }
template<typename Task, typename D1, typename D2>
auto 
testDTImpl(stdx::choice<1>, Task& t, D1& d1, const D2& d2, ManageStore& m)
    -> stdx::if_compiles_return<std::true_type,decltype(doTask(t,d1,d2,m))>
    {
    return std::true_type{};
    }
template<typename Task, typename D1, typename D2>
struct HasDTHelper2Arg
    {
    Task* t;
    D1* d1;
    D2* d2;
    ManageStore* m;
    auto operator()() -> decltype(testDTImpl(stdx::select_overload{},*t,*d1,*d2,*m))
        { return testDTImpl(stdx::select_overload{},*t,*d1,*d2,*m); }
    };
template<typename Task, typename D1, typename D2>
struct HasConstDoTask2Arg
    {
    using ResultType = stdx::result_of_t<HasDTHelper2Arg<Task,const D1, const D2>()>;
    bool constexpr static
    result() { return ResultType{}; }
    };
template<typename Task, typename D1, typename D2>
struct HasNonConstDoTask2Arg
    {
    using ResultType = stdx::result_of_t<HasDTHelper2Arg<Task,stdx::remove_const_t<D1>,D2>()>;
    using CResultType = stdx::result_of_t<HasDTHelper2Arg<Task,const D1,D2>()>;
    bool constexpr static
    result() { return ResultType{} && (not CResultType{}); }
    };
template<typename Task, typename D1, typename D2>
struct HasDoTask2Arg
    {
    using CResultType = stdx::result_of_t<HasDTHelper2Arg<Task,const D1,D2>()>;
    using NCResultType = stdx::result_of_t<HasDTHelper2Arg<Task,stdx::remove_const_t<D1>,D2>()>;
    using ResultType = stdx::conditional_t<CResultType::value,
                                          CResultType,
                                          NCResultType>;
    bool constexpr static
    result() { return ResultType{}; }
    };

/////////////////////

template<typename D>
std::false_type 
testEvalImpl(stdx::choice<2>, D& d)
    {
    return std::false_type{};
    }
template<typename D>
auto
testEvalImpl(stdx::choice<1>, D& d)
    -> stdx::if_compiles_return<std::true_type,decltype(evaluate(d))>
    {
    return std::true_type{};
    }
template<typename Storage>
struct HasEvaluate
    {
    struct Test 
        {
        Storage* s;
        auto operator()() -> decltype(testEvalImpl(stdx::select_overload{},*s))
            { return testEvalImpl(stdx::select_overload{},*s); }
        };
    bool constexpr static
    result() { return stdx::result_of_t<Test()>{}; }
    };

/////////////

template<typename D>
PData
callEvaluateImpl(stdx::choice<2>, D& d)
    {
    throw std::runtime_error("No doTask overload found for task/storage type");
    return PData{};
    }
template<typename D>
auto
callEvaluateImpl(stdx::choice<1>, D& d)
    -> stdx::if_compiles_return<decltype(evaluate(d)),PData>
    {
    return evaluate(d);
    }
template<typename D>
PData
callEvaluate(D& d)
    {
    return callEvaluateImpl(stdx::select_overload{},d);
    }

/////////////

template<typename D>
bool constexpr
checkHasResultImpl(stdx::choice<2>, const D& d)
    {
    return true;
    }
template<typename D>
auto
checkHasResultImpl(stdx::choice<1>, const D& d)
    -> stdx::if_compiles_return<bool,decltype(hasResult(d))>
    {
    return hasResult(d);
    }
template<typename D>
bool constexpr
checkHasResult(const D& d)
    {
    return checkHasResultImpl(stdx::select_overload{},d);
    }

/////////////


void inline
check(const PData& p)
    {
    if(!p) Error("doTask called on unallocated store pointer");
    }
void inline
check(const CPData& p)
    {
    if(!p) Error("doTask called on unallocated store pointer");
    }

/////////////

template <typename NArgs, typename Task, typename Return>
template<typename D>
void RegisterTask<NArgs,Task,Return>::
applyToImpl(D& d)
    {
    constexpr bool isLazy = HasEvaluate<D>::result();
    if(isLazy)
        {
        if(checkHasResult(d))
            {
            println("Lazy storage has result, swapping");
            m_.parg1() = callEvaluate(d);
            m_.parg1()->plugInto(*this);
            return;
            }
        }
    NArgs{}.call(*this,task_,d,m_,ret_);
    }

template<class PType>
template<typename RT, typename Task, typename D, typename Return>
void OneArg<PType>::
call(RT& rt, Task& t, D& d, ManageStore& m, Return& ret)
    {
    constexpr bool NCData = std::is_same<PType,PData>::value;
    constexpr bool hasConstDT = HasConstDoTask<Task,D>::result();
    constexpr bool hasNCDT = HasNonConstDoTask<Task,D>::result();
    constexpr bool isLazy = HasEvaluate<D>::result();
    if(hasConstDT)
        {
        const auto& cd = d;
        detail::callDoTask(t,cd,m,ret);
        }
    else if(NCData && hasNCDT)
        {
        auto* pd = m.modifyData(d);
        detail::callDoTask(t,*pd,m,ret);
        }
    else if(isLazy)
        {
        m.parg1() = callEvaluate(d);
        m.parg1()->plugInto(rt);
        }
    else
        {
        throw ITError("doTask not defined for task/storage type [4]");
        }
    }

template<class PType1, class PType2>
template<typename RT, typename Task, typename D, typename Return>
void TwoArgs<PType1,PType2>::
call(RT& rt, Task& t, D& d, ManageStore& m, Return& ret)
    {
    CallWrap<RT,Task,D,Return,PType1,PType2> w(rt,t,d,m,ret);
    m.parg2()->plugInto(w);
    }

template <class RT, typename Task, typename D1, typename Return, class PType1, class PType2>
template<typename D2>
void CallWrap<RT,Task,D1,Return,PType1,PType2>::
applyToImpl(D2& d2)
    { 
    constexpr bool isLazy2 = HasEvaluate<D2>::result();
    if(isLazy2)
        {
        if(checkHasResult(d2))
            {
            println("Lazy storage has result, swapping");
            m_.parg2() = callEvaluate(d2);
            m_.parg2()->plugInto(*this);
            return;
            }
        }

    constexpr bool NCData1 = std::is_same<PType1,PData>::value;
    constexpr bool hasCDT = HasConstDoTask2Arg<Task,D1,D2>::result();
    constexpr bool hasNCDT = HasNonConstDoTask2Arg<Task,D1,D2>::result();
    constexpr bool isLazy1 = HasEvaluate<D1>::result();
    if(hasCDT)
        {
        const auto& cd1 = d1_;
        callDoTask(t_,cd1,d2,m_,ret_);
        }
    else if(NCData1 && hasNCDT)
        {
        auto* pd1 = m_.modifyData(d1_);
        callDoTask(t_,*pd1,d2,m_,ret_);
        }
    else if(isLazy1 && isLazy2)
        {
        m_.parg1() = callEvaluate(d1_);
        m_.parg2() = callEvaluate(d2);
        m_.parg1()->plugInto(rt_);
        }
    else if(isLazy1)
        {
        constexpr bool hasCPDT = HasConstDoTask2Arg<Task,D1,const PData>::result();
        constexpr bool hasNCPDT = HasNonConstDoTask2Arg<Task,D1,const PData>::result();
        if(hasCPDT)
            {
            const auto& cd1 = d1_;
            callDoTask(t_,cd1,m_.parg2(),m_,ret_);
            }
        else if(NCData1 && hasNCPDT)
            {
            auto* pd1 = m_.modifyData(d1_);
            callDoTask(t_,*pd1,m_.parg2(),m_,ret_);
            }
        else
            {
            m_.parg1() = callEvaluate(d1_);
            m_.parg1()->plugInto(rt_);
            }
        }
    else if(isLazy2)
        {
        constexpr bool hasPDT = HasConstDoTask2Arg<Task,const PData,D2>::result();
        if(hasPDT)
            {
            callDoTask(t_,m_.parg1(),d2,m_,ret_);
            }
        else
            {
            m_.parg2() = callEvaluate(d2);
            m_.parg2()->plugInto(*this);
            }
        }
    else
        {
        throw ITError("doTask not defined for task/storage type [6]");
        }
    }

template<typename Task, typename TList>
struct GetRType : GetRType<Task,popFront<TList>>
    {
    using Test = stdx::result_of_t<TestRet<Task,frontType<TList>>()>;
    using Parent = GetRType<Task,popFront<TList>>;
    using RType = stdx::conditional_t<not std::is_same<Test,NoneType>::value,
                                     Test,
                                     typename Parent::RType>;
    };

template<typename Task>
struct GetRType<Task,TypeList<>>
    {
    using RType = NoneType;
    };

} //namespace detail

template<typename Task, typename TList>
using ReturnType = typename detail::GetRType<stdx::remove_reference_t<Task>,TList>::RType;

//////
////// doTask methods
//////

template<typename T, typename... VArgs>
PData
newITData(VArgs&&... vargs)
    {
    //static_assert(containsType<StorageTypes,T>{},"Data type not in list of registered storage types");
    return std::make_shared<ITWrap<T>>(std::forward<VArgs>(vargs)...);
    }

template<typename Task>
auto
doTask(Task&& t,
       CPData arg)
    -> typename detail::RegisterTask<detail::OneArg<CPData>,Task,ReturnType<Task,StorageTypes>>::return_type
    {
#ifdef DEBUG
    detail::check(arg);
#endif
    using Ret = ReturnType<Task,StorageTypes>;
    ManageStore m(&(arg.p));
    detail::RegisterTask<detail::OneArg<CPData>,Task,Ret> r{std::forward<Task>(t),std::move(m)};
    arg->plugInto(r);
    return r.getReturn();
    }

template<typename Task>
auto
doTask(Task&& t,
       PData& arg)
    -> typename detail::RegisterTask<detail::OneArg<PData>,Task,ReturnType<Task,StorageTypes>>::return_type
    {
#ifdef DEBUG
    detail::check(arg);
#endif
    using Ret = ReturnType<Task,StorageTypes>;
    ManageStore m(&arg);
    detail::RegisterTask<detail::OneArg<PData>,Task,Ret> r(std::forward<Task>(t),std::move(m));
    arg->plugInto(r);
    return r.getReturn();
    }

template<typename Task>
auto
doTask(Task&& t,
       CPData arg1,
       CPData arg2)
    -> typename detail::RegisterTask<detail::TwoArgs<CPData,CPData>,Task,ReturnType<Task,StorageTypes>>::return_type
    {
#ifdef DEBUG
    detail::check(arg1);
    detail::check(arg2);
#endif
    using Ret = ReturnType<Task,StorageTypes>;
    ManageStore m(&(arg1.p),&(arg2.p));
    detail::RegisterTask<detail::TwoArgs<CPData,CPData>,Task,Ret> r(std::forward<Task>(t),std::move(m));
    arg1->plugInto(r);
    return r.getReturn();
    }

template<typename Task>
auto
doTask(Task&& t,
       PData& arg1,
       CPData arg2)
    -> typename detail::RegisterTask<detail::TwoArgs<PData,CPData>,Task,ReturnType<Task,StorageTypes>>::return_type
    {
#ifdef DEBUG
    detail::check(arg1);
    detail::check(arg2);
#endif
    using Ret = ReturnType<Task,StorageTypes>;
    ManageStore m(&arg1,&(arg2.p));
    detail::RegisterTask<detail::TwoArgs<PData,CPData>,Task,Ret> r(std::forward<Task>(t),std::move(m));
    arg1->plugInto(r);
    return r.getReturn();
    }


template<typename F>
F
applyFunc(F&& f, PData& store)
    {
    doTask(detail::ApplyFunc<F,void>{std::forward<F>(f)},store);
    return f;
    }

template<typename F>
F
applyFunc(F&& f, const CPData& store)
    {
    doTask(detail::ApplyFunc<F,void>{std::forward<F>(f)},store);
    return f;
    }

template<typename Ret, typename F>
Ret
applyFunc(F&& f, PData& store)
    {
    return doTask(detail::ApplyFunc<F,Ret>{std::forward<F>(f)},store);
    }

template<typename Ret, typename F>
Ret
applyFunc(F&& f, const CPData& store)
    {
    return doTask(detail::ApplyFunc<F,Ret>{std::forward<F>(f)},store);
    }

} //namespace itensor

#undef REGISTER_ITDATA_HEADER_FILES

#endif

