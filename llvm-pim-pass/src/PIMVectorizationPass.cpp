#include <array>
#include <list>
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <iostream>

using namespace llvm;

namespace
{

class PIMVectorizationPass : public FunctionPass
{
public:
    static char ID;

private:
    static const int PIM_VECTOR_SIZE = 65536;
    static const int MAX_TRANSP_SEARCH_DEPTH = 100;
    static const int MEMALIGN_TYPE_COUNT = 4;

    enum PIMInstrType {
        PIMAnd = 0,
        PIMOr,
        PIMNot,
        PIMAdd,
        PIMSub,
        PIMMul,
        PIMDiv,
        PIM_INSTR_TYPE_COUNT
    };

    struct DependencyGraphNode {
        bool isPIMInstr;
        Instruction *instr;
        Value *dest;
        Instruction *pos;
        DependencyGraphNode *op1;
        DependencyGraphNode *op2;
    };
    std::list<DependencyGraphNode> dependencyGraph;

    Function *currentFunction;
    Module *currentModule;

    std::array<Function*, PIM_INSTR_TYPE_COUNT> PIMFunc;
    std::array<bool, PIM_INSTR_TYPE_COUNT> PIMInstrUsed;

    const std::array<std::string, PIM_INSTR_TYPE_COUNT> PIMInstrName = {
            "__llvm_PIM_and",
            "__llvm_PIM_or",
            "__llvm_PIM_not",
            "__llvm_PIM_add",
            "__llvm_PIM_sub",
            "__llvm_PIM_mul",
            "__llvm_PIM_div"
    };
    const std::array<std::string, PIM_INSTR_TYPE_COUNT> PIMInstrOpcode = {
            "0x5C",
            "0x5D",
            "0x5E",
            "0x62",
            "0x63",
            // TODO Those are probably wrong
            "0x64",
            "0x65"
    };
    const std::array<int, PIM_INSTR_TYPE_COUNT> PIMInstrArgCount = { 2, 2, 1, 2, 2, 2, 2 };

    // If we didn't use a set we would have to check for duplicates
    std::set<Instruction*> instrDelList;
    std::list<std::pair<Instruction*, Instruction*> > instrInsertList;
    std::list<std::pair<Instruction*, Instruction*> > instrReplaceUsesList;
    std::set<Value*> transposedValues;
    std::set<Value*> pimMultipleValues;
    std::set<Value*> recursionValues;
    Instruction *memalignInst[MEMALIGN_TYPE_COUNT];
    Instruction *memalignBasePtr[MEMALIGN_TYPE_COUNT];
    unsigned int memalignAllocCount[MEMALIGN_TYPE_COUNT];

public:
    PIMVectorizationPass()
        : FunctionPass(ID),
          PIMFunc(),
          PIMInstrUsed({ false, false, false }),
          currentModule(nullptr),
          currentFunction(nullptr)
    {
        for (int i = 0; i < MEMALIGN_TYPE_COUNT; i++)
        {
            this->memalignAllocCount[i] = 0;
            this->memalignBasePtr[i] = nullptr;
            this->memalignInst[i] = nullptr;
        }
    }

    bool runOnFunction(Function &func) override
    {
        // We don't want to propagate that reference through all functions
        this->currentFunction = &func;

        // Scan for PIM suited instructions and create the dependency graph
        for (auto &block : func)
            for (auto &instr : block)
                scanForPIMInstruction(instr);

        // Resolve the dependencies and mark instructions for replacement/deletion
        for (auto &node : this->dependencyGraph)
            resolveDependencyNode(node);
        this->dependencyGraph.clear();

        // Keep track on whether we change code or not
        bool changed =    !this->instrInsertList.empty()
                       || !this->instrDelList.empty()
                       || !this->instrReplaceUsesList.empty();

        // We don't modify the code in the loop cause it might
        // break the iterators.

        // Insert the new PIM call instructions
        for (auto &pair : this->instrInsertList)
            pair.second->insertBefore(pair.first);
        this->instrInsertList.clear();

        // Replace uses of the old vector instructions
        for (auto &pair : this->instrReplaceUsesList)
            pair.first->replaceAllUsesWith(pair.second);
        this->instrReplaceUsesList.clear();

        // Remove the old vector instructions
        for (auto *instr : this->instrDelList)
            instr->eraseFromParent();
        this->instrDelList.clear();

        // Modify the posix_memalign function to allocate the right amount of memory
        this->fixMemaligns();

        return changed;
    }

    bool doFinalization(Module &module) override
    {
        bool ret = false;

        // If we never used a particular PIM function, we might as well remove it again
        ret = this->removeUnusedPIMFunction(module, PIMAnd) || ret;
        ret = this->removeUnusedPIMFunction(module, PIMOr)  || ret;
        ret = this->removeUnusedPIMFunction(module, PIMNot) || ret;
        ret = this->removeUnusedPIMFunction(module, PIMAdd) || ret;
        ret = this->removeUnusedPIMFunction(module, PIMSub) || ret;
        ret = this->removeUnusedPIMFunction(module, PIMMul) || ret;
        ret = this->removeUnusedPIMFunction(module, PIMDiv) || ret;

        return ret;
    }

private:
    bool doInitialization(Module &module) override
    {
        // We don't want to propagate that reference through all functions
        this->currentModule = &module;

        // Create the functions that get called for PIM instructions
        this->createPIMInstr(PIMAnd);
        this->createPIMInstr(PIMOr);
        this->createPIMInstr(PIMNot);
        this->createPIMInstr(PIMAdd);
        this->createPIMInstr(PIMSub);
        this->createPIMInstr(PIMMul);
        this->createPIMInstr(PIMDiv);

        // We changed the module
        return true;
    }

    void fixMemaligns()
    {
        for (int i = 0; i < MEMALIGN_TYPE_COUNT; i++)
        {
            if (this->memalignAllocCount[i] > 0)
            {
                // Get the data layout of the architecture to calculate type sizes
                const DataLayout &dataLayout = this->currentModule->getDataLayout();
                Type *ptrType = dataLayout.getIntPtrType(this->currentFunction->getContext());

                // Calculate the total memory needed
                auto *newAllocConst = ConstantInt::get(ptrType, this->memalignAllocCount[i] * PIM_VECTOR_SIZE * (i + 1));

                // Find the correct operand and modify it
                auto *op = this->memalignInst[i]->op_begin() + 2;
                *op = newAllocConst;

                this->memalignAllocCount[i] = 0;
                this->memalignBasePtr[i] = nullptr;
                this->memalignInst[i] = nullptr;
            }
        }
    }

    // Add a global PIM function that we can call
    void createPIMInstr(PIMInstrType instrType)
    {
        LLVMContext &context = this->currentModule->getContext();
        const int argCount = this->PIMInstrArgCount[instrType];

        // The arguments of the function are op1, op2 and dst, all int pointers
        auto *arg_type = PointerType::getUnqual(VectorType::get(Type::getInt32Ty(context), PIM_VECTOR_SIZE, false));
        std::vector<Type*> arg_types;
        arg_types.assign(argCount + 1, arg_type);

        // Create the function prototype
        auto functype = FunctionType::get(Type::getVoidTy(context), arg_types, false);

        // Add the function
        this->PIMFunc[instrType] = Function::Create(functype,
                                                    Function::LinkOnceODRLinkage,
                                                    this->PIMInstrName[instrType],
                                                    this->currentModule);

        // Try very hard to not get that function optimized away
        this->PIMFunc[instrType]->addFnAttr(Attribute::NoInline);
        this->PIMFunc[instrType]->addFnAttr(Attribute::OptimizeNone);
        this->PIMFunc[instrType]->addFnAttr(Attribute::NoUnwind);

        // Add a basic block to the body of our function
        BasicBlock* block = BasicBlock::Create(context, "entry", this->PIMFunc[instrType]);

        // Construct the arguments to the asm call
        std::vector<Value*> args;
        args.reserve(argCount + 1);
        for (int i = 0; i < argCount + 1; i++)
            args.push_back(this->PIMFunc[instrType]->getArg(i));

        // Construct the asm constraints
        std::string asmConstraints = "=*m";
        for (int i = 0; i < argCount; i++)
            asmConstraints.append(",*m");

        // Add the PIM instructions to the body of the function
        IRBuilder<> builder(block);
        auto *asm_func_type = FunctionType::get(arg_type, arg_types, false);
        auto *asm_code = InlineAsm::get(asm_func_type,
                                        ".byte 0x0F, 0x04;\n"
                                        ".word " + this->PIMInstrOpcode[instrType] + ";",
                                        asmConstraints, true);
        builder.CreateCall(asm_func_type, asm_code, args, this->PIMInstrName[instrType] + "_asm");
        builder.CreateRetVoid();
    }

    void scanForPIMInstruction(Instruction &instr)
    {
        // Check if it is a call to one of the special transposition functions
        checkForTransposeInstr(&instr);

        // Check whether the instruction has the correct vector size
        auto *type = dyn_cast<FixedVectorType>(instr.getType());
        if (!type || type->getNumElements() != PIM_VECTOR_SIZE)
            return;

        // Add supported operations to the dependency graph
        ConstantDataVector *constVec;
        switch (instr.getOpcode())
        {
            case Instruction::And:
            case Instruction::Or:
            case Instruction::Add:
            case Instruction::Sub:
            case Instruction::Mul:
            case Instruction::SDiv:
            case Instruction::UDiv:
                // All binary operations are handled exactly the same
                addBinaryOpToDependencyGraph(instr, type->getElementType());
                break;
            case Instruction::Xor:
                // LLVM doesn't support a bitwise NOT operation, it always uses an XOR instead
                // Check whether the XOR is actually doing a bitwise NOT operation
                constVec = dyn_cast<ConstantDataVector>(instr.getOperand(1));
                if (!constVec)
                    break;
                for (unsigned int i = 0; i < constVec->getNumElements(); i++)
                    // It has to be all ones but with the correct integer size
                    if (constVec->getElementAsInteger(i) != ((uint64_t)-1) >> (64 - type->getElementType()->getIntegerBitWidth()))
                        break;
                addUnaryOpToDependencyGraph(instr, type->getElementType());
                break;
            default:
                break;
        }
    }

    void addBinaryOpToDependencyGraph(Instruction &instr, Type *elementType)
    {
        // Initialize the dependency graph node
        struct DependencyGraphNode node {
                .isPIMInstr = true,
                .instr = &instr,
                .dest = nullptr,
                .pos = nullptr,
                .op1 = nullptr,
                .op2 = nullptr
        };

        // Get the operands of the instruction
        auto *op1 = dyn_cast<Instruction>(instr.getOperand(0));
        auto *op2 = dyn_cast<Instruction>(instr.getOperand(1));

        // I'm reasonably sure that LLVM won't generate large vector type constants or
        // arguments that aren't the result of an instruction.
        // If it does, we're out of luck (for now).
        if (op1 == nullptr || op2 == nullptr)
            return;

        // Both operands must be transposed or we can't replace the instruction
        // We handle load instructions specially, as those are only allowed once at the start
        if ((!dyn_cast<LoadInst>(op1) || !ensureTransposed(op1->getOperand(0))) && !ensureTransposed(op1))
            return;

        if ((!dyn_cast<LoadInst>(op2) || !ensureTransposed(op2->getOperand(0))) && !ensureTransposed(op2))
            return;

        // Search for the operands in the dependency graph
        for (auto &n : this->dependencyGraph)
        {
            // Save the dependency graph nodes for the operands instead of just
            // the operands themselves.
            if (n.instr == op1)
                node.op1 = &n;
            if (n.instr == op2)
                node.op2 = &n;
        }

        // If the operand isn't a PIM instruction, create a scalar dependency for it
        if (node.op1 == nullptr)
            node.op1 = addScalarOperandDependency(op1, &instr, elementType);

        if (node.op2 == nullptr)
            node.op2 = addScalarOperandDependency(op2, &instr, elementType);

        // Change the destination to be PIM suitable
        node.dest = changeDestToPIM(&instr, elementType, &node.pos);

        // Mark the result as transposed
        // We shouldn't have to add a transposition instruction as that should happen automatically
        this->transposedValues.emplace(node.dest);

        // Mark the result for the old instruction as transposed as well, as that is what
        // instructions further down the chain see
        this->transposedValues.emplace(&instr);

        this->dependencyGraph.push_back(node);
    }

    void addUnaryOpToDependencyGraph(Instruction &instr, Type *elementType)
    {
        // Initialize the dependency graph node
        struct DependencyGraphNode node {
                .isPIMInstr = true,
                .instr = &instr,
                .dest = nullptr,
                .pos = nullptr,
                .op1 = nullptr,
                .op2 = nullptr
        };

        // Get the operand of the instruction
        auto *op = dyn_cast<Instruction>(instr.getOperand(0));

        // I'm reasonably sure that LLVM won't generate large vector type constants or
        // arguments that aren't the result of an instruction.
        // If it does, we're out of luck (for now).
        if (op == nullptr)
            return;

        // The operand must be transposed or we can't replace the instruction
        if ((!dyn_cast<LoadInst>(op) || !ensureTransposed(op->getOperand(0))) && !ensureTransposed(op))
            return;

        // Search for the operands in the dependency graph
        for (auto &n : this->dependencyGraph)
        {
            // Save the dependency graph nodes for the operands instead of just
            // the operands themselves.
            if (n.instr == op)
                node.op1 = &n;
        }

        // If the operand isn't a PIM instruction, create a scalar dependency for it
        if (node.op1 == nullptr)
            node.op1 = addScalarOperandDependency(op, &instr, elementType);

        // Change the destination to be PIM suitable
        node.dest = changeDestToPIM(&instr, elementType, &node.pos);

        // Mark the result as transposed
        // We shouldn't have to add a transposition instruction as that should happen automatically
        this->transposedValues.emplace(node.dest);

        // Mark the result for the old instruction as transposed as well, as that is what
        // instructions further down the chain see
        this->transposedValues.emplace(&instr);

        this->dependencyGraph.push_back(node);
    }

    void checkForTransposeInstr(Instruction *instr)
    {
        // The transpose instructions are calls to special external functions
        auto *callInstr = dyn_cast<CallInst>(instr);
        if (callInstr)
        {
            auto *func = callInstr->getCalledFunction();
            if (func)
            {
                auto funcName = func->getName();
                // Marks a value as transposed, even though we might not find the original
                // transposition instruction
                if (funcName == "simdram_mark_transposed")
                {
                    auto *value = instr->getOperand(0);
                    this->transposedValues.emplace(value);
                    // Those functions don't actually exist and are only to mark a value,
                    // so we have to delete them
                    this->instrDelList.emplace(instr);
                    // Often times the operand will be a bitcast which we wouldn't find during
                    // traversal, so we already mark it as well
                    auto *bitcastOp = dyn_cast<BitCastInst>(value);
                    if (bitcastOp)
                    {
                        this->transposedValues.emplace(bitcastOp->getOperand(0));
                        if (bitcastOp->getNumUses() == 1)
                            this->instrDelList.emplace(bitcastOp);
                    }
                }
                else if (funcName == "simdram_transpose")
                {
                    addTranspInstr(instr, instr);
                    // Those functions don't actually exist and are only to mark a value,
                    // so we have to delete them
                    this->instrDelList.emplace(instr);
                }
            }
        }
    }

    bool ensureTransposed(Value *value)
    {
        // TODO I couldn't figure out yet how to rollback a transposition, so for now if the transposition
        // for the first operand succeeds but fails for the second operand, the first operand will stay
        // transposed.

//        // Create a snapshot of the current changes so we can revert them if needed
//        auto snapAllocSize = this->memalignAllocCount;
//        auto snapMemalign = this->memalignInst;
//        auto snapMemalignBase = this->memalignBasePtr;
//        auto snapInsert = this->instrInsertList.end();
//        auto snapDelete = this->instrDelList.end();
//        auto snapReplace = this->instrReplaceUsesList.end();
//        auto snapTransp = this->transposedValues.end();
//        if (!this->instrInsertList.empty())
//            snapInsert--;
//        if (!this->instrDelList.empty())
//            snapDelete--;
//        if (!this->instrReplaceUsesList.empty())
//            snapReplace--;
//        if (!this->transposedValues.empty())
//            snapTransp--;

        this->recursionValues.clear();
        bool ret = ensureTransposedRec(value, 0);

//        // We might have changed something even though not everything is transposed, in
//        // which case we want to revert those changes
//        if (!ret)
//        {
//            if (snapInsert == this->instrInsertList.end())
//                snapInsert = this->instrInsertList.begin();
//            else
//                snapInsert++;
//            if (snapDelete == this->instrDelList.end())
//                snapDelete = this->instrDelList.begin();
//            else
//                snapDelete++;
//            if (snapReplace == this->instrReplaceUsesList.end())
//                snapReplace = this->instrReplaceUsesList.begin();
//            else
//                snapReplace++;
//            if (snapTransp == this->transposedValues.end())
//                snapTransp = this->transposedValues.begin();
//            else
//                snapTransp++;
//            for (auto instr = snapInsert; instr != this->instrInsertList.end(); instr++)
//                instr->second->deleteValue();
//            for (auto instr = snapInsert; instr != this->instrReplaceUsesList.end(); instr++)
//                instr->second->deleteValue();
//            this->instrInsertList.erase(snapInsert, this->instrInsertList.end());
//            this->instrDelList.erase(snapDelete, this->instrDelList.end());
//            this->instrReplaceUsesList.erase(snapReplace, this->instrReplaceUsesList.end());
//            this->memalignAllocCount = snapAllocSize;
//            this->memalignInst = snapMemalign;
//            this->memalignBasePtr = snapMemalignBase;
//        }

        return ret;
    }

    bool ensureTransposedRec(Value *value, unsigned int depth)
    {
        if (depth > MAX_TRANSP_SEARCH_DEPTH)
            return false;

        // Check if we already marked this value as transposed before
        if (this->transposedValues.find(value) != this->transposedValues.end())
            return true;

        // If the value isn't directly marked already, we try to backtrace until we
        // either can't backtrace further or found a marked value
        auto *instr = dyn_cast<Instruction>(value);

        if (instr)
        {
            Function *func;
            PHINode *phi;
            Instruction *inselement, *store;
            ConstantInt *zeroScalar;
            Value *scalar, *mem;
            FixedVectorType *vecInstType;
            struct DependencyGraphNode node {
                    .isPIMInstr = false,
                    .instr = nullptr,
                    .dest = nullptr,
                    .pos = nullptr,
                    .op1 = nullptr,
                    .op2 = nullptr
            };

            // If it's an instruction, we might be able to backtrace further if it
            // doesn't change the value of the pointer in unpredictable ways
            switch (instr->getOpcode())
            {
                case Instruction::BitCast:
                    // Bitcasts are transparent
                    if (ensureTransposedRec(instr->getOperand(0), depth + 1))
                    {
                        this->transposedValues.emplace(value);
                        return true;
                    }
                    return false;

                case Instruction::GetElementPtr:
                    // We are using instr->getOperand(instr->getNumOperands() - 1) here because
                    // getelementptr can have multiple indices. I am about 90% sure that only checking the last
                    // index should be enough and that memory should always stay aligned this way.
                    if (ensureTransposedRec(instr->getOperand(0), depth + 1)
                        && isMultipleOfPIMSize(instr->getOperand(instr->getNumOperands() - 1), depth + 1))
                    {
                        this->transposedValues.emplace(value);
                        return true;
                    }
                    return false;

                case Instruction::PHI:
                    phi = dyn_cast<PHINode>(value);
                    // Phi values often become recursive, so we have to catch that
                    if (recursionValues.find(phi) != recursionValues.end())
                    {
                        // The other operand should be checked by the first time we checked the instruction
                        // We will also mark it as transposed there
                        return true;
                    }

                    depth += phi->getNumIncomingValues();
                    recursionValues.emplace(phi);
                    for (auto &op : phi->incoming_values())
                    {
                        // Make sure not to use recursionValues after it was moved!
                        if (!ensureTransposedRec(op.getUser(), depth))
                            return false;
                    }
                    this->transposedValues.emplace(value);
                    return true;

                case Instruction::Call:
                    func = dyn_cast<CallInst>(instr)->getCalledFunction();
                    if (!func)
                        return false;

                    // Intrinsics aren't allowed as we would have to handle each of them
                    // individually and they shouldn't commonly occur in our path
                    if (func->isIntrinsic())
                        return false;

                    // If we found a malloc, we can add a transposition and be happy
                    if (func->getName() == "malloc")
                    {
                        // To make sure the allocated memory is not only transposed but also aligned, we replace
                        // the malloc with a posix_memalign
                        auto [loadInst, memalignInst] = createMemalignInstr(instr->getOperand(0),
                                                                            instr, instr->getType());
                        this->instrDelList.emplace(instr);
                        this->instrReplaceUsesList.emplace_back(instr, loadInst);
                        this->transposedValues.emplace(value);
                        return true;
                    }

                    // Prevent infinite recursion
                    if (this->recursionValues.find(func) != this->recursionValues.end())
                        return true;
                    this->recursionValues.emplace(func);

                    // The entry point to backtrace that function are the return statements
                    // The depth counter might become off here, which is hopefully not a problem
                    for (auto &block : *func)
                        for (auto &i : block)
                            if (auto *ret = dyn_cast<ReturnInst>(&i))
                                if (!ensureTransposedRec(ret->getOperand(0), depth + 1))
                                    return false;

                    // Now that we are done with that particular function call, we can allow it
                    // to be called again
                    this->recursionValues.erase(this->recursionValues.find(func));

                    this->transposedValues.emplace(value);
                    return true;

                case Instruction::Add:
                case Instruction::Sub:
                    if (auto *c1 = dyn_cast<ConstantInt>(instr->getOperand(0)))
                    {
                        if (c1->getValue().urem(PIM_VECTOR_SIZE) != 0)
                            return false;

                        if (!ensureTransposedRec(instr->getOperand(1), depth + 1))
                            return false;

                        this->transposedValues.emplace(value);
                        return true;
                    }
                    else if (auto *c2 = dyn_cast<ConstantInt>(instr->getOperand(1)))
                    {
                        if (c2->getValue().urem(PIM_VECTOR_SIZE) != 0)
                            return false;

                        if (!ensureTransposedRec(instr->getOperand(0), depth + 1))
                            return false;

                        this->transposedValues.emplace(value);
                        return true;
                    }

                    return false;

                case Instruction::ShuffleVector:
                    // This is only used in case the operand is a vector filled with one scalar in all elements
                    // The pattern looks like this:
                    // %1 = insertelement <65536 x i32> poison, i32 %0, i32 0
                    // %2 = shufflevector <65536 x i32> %1, <65536 x i32> poison, <65536 x i32> zeroinitializer
                    vecInstType = dyn_cast<FixedVectorType>(instr->getType());
                    if (!vecInstType || vecInstType->getNumElements() != PIM_VECTOR_SIZE)
                        return false;
                    if (!dyn_cast<PoisonValue>(instr->getOperand(1)))
                        return false;
                    for (auto v : dyn_cast<ShuffleVectorInst>(instr)->getShuffleMask())
                        if (v != 0)
                            return false;
                    inselement = dyn_cast<InsertElementInst>(instr->getOperand(0));
                    if (!inselement)
                        return false;
                    if (!dyn_cast<PoisonValue>(inselement->getOperand(0)))
                        return false;
                    zeroScalar = dyn_cast<ConstantInt>(inselement->getOperand(2));
                    if (!zeroScalar || zeroScalar->getValue() != 0)
                        return false;
                    scalar = inselement->getOperand(1);
                    // I believe we don't have to typecheck this further

                    // Storing the result of this in memory is already handled when creating
                    // a scalar dependency.
                    this->transposedValues.emplace(value);
                    return true;

                default:
                    return false;
            }
        }
        else
        {
            // If the value isn't an instruction, the only way to backtrace further is
            // if it's a function argument
            auto *arg = dyn_cast<Argument>(value);
            if (!arg)
                return false;

            unsigned int argNo = arg->getArgNo();
            Function *func = arg->getParent();

            // We want to limit the amount of checks to perform, if the function has too many
            // uses, we just discard it as not suitable
            depth += func->getNumUses();
            if (depth > MAX_TRANSP_SEARCH_DEPTH)
                return false;

            // If it's an argument, check every use and make sure all of them only
            // use correctly marked memory
            for (auto &use : func->uses())
            {
                auto *funcUse = dyn_cast<Instruction>(use.getUser());

                // NOTE This fails if we e.g. mark the function as used
                if (!funcUse)
                    return false;

                // Backtrace further
                if (!ensureTransposedRec(funcUse->getOperand(argNo), depth))
                    return false;
            }

            this->transposedValues.emplace(value);
            return true;
        }
    }

    bool isMultipleOfPIMSize(Value *value, unsigned int depth, std::set<PHINode*> phiValues = {})
    {
        if (depth > MAX_TRANSP_SEARCH_DEPTH)
            return false;

        if (this->pimMultipleValues.find(value) != this->pimMultipleValues.end())
            return true;

        if (auto *constInt = dyn_cast<ConstantInt>(value))
        {
            if (constInt->getValue().urem(PIM_VECTOR_SIZE) == 0)
            {
                this->pimMultipleValues.emplace(value);
                return true;
            }
            return false;
        }

        if (auto *arg = dyn_cast<Argument>(value))
        {
            unsigned int argNo = arg->getArgNo();
            Function *func = arg->getParent();

            // We want to limit the amount of checks to perform, if the function has too many
            // uses, we just discard it as not suitable
            depth += func->getNumUses();
            if (depth > MAX_TRANSP_SEARCH_DEPTH)
                return false;

            // Check every use of the function
            for (auto &use : func->uses())
            {
                auto *funcUse = dyn_cast<Instruction>(use.getUser());

                // NOTE This fails if we e.g. mark the function as used
                if (!funcUse)
                    return false;

                // Backtrace further
                if (!isMultipleOfPIMSize(funcUse->getOperand(argNo), depth, phiValues))
                    return false;
            }

            this->pimMultipleValues.emplace(value);
            return true;
        }

        auto *instr = dyn_cast<Instruction>(value);
        if (!instr)
            return false;

        if ((instr->getOpcode() == Instruction::Add || instr->getOpcode() == Instruction::Sub))
        {
            if (isMultipleOfPIMSize(instr->getOperand(0), depth + 1, phiValues)
                || isMultipleOfPIMSize(instr->getOperand(1), depth + 1, phiValues))
            {
                this->pimMultipleValues.emplace(value);
                return true;
            }
            return false;
        }

        if (instr->getOpcode() == Instruction::PHI)
        {
            auto *phi = dyn_cast<PHINode>(instr);

            // Phi values often become recursive, so we have to catch that
            if (phiValues.find(phi) != phiValues.end())
            {
                // The other operand should be checked by the first time we checked the instruction
                // We will also mark it as transposed there
                return true;
            }

            depth += phi->getNumIncomingValues();
            phiValues.emplace(phi);
            for (auto &op : phi->incoming_values())
                if (!isMultipleOfPIMSize(op.get(), depth, phiValues))
                    return false;

            this->pimMultipleValues.emplace(value);
            return true;
        }

        return false;
    }

    static void addTranspInstr(Value *addr, Instruction *insertAfter)
    {
        (void) addr;
        (void) insertAfter;
        // TODO Actually transpose the memory (needs gem5 support first)
    }

    Value *changeDestToPIM(Instruction *instr, Type *elementType, Instruction **pos)
    {
        // Search each use and check if it's a store address we can use as a destination
        for (auto &use : instr->uses())
        {
            // Check if the use is a store
            auto *store = dyn_cast<StoreInst>(use.getUser());
            if (store == nullptr)
                continue;

            // There might be illegal instructions that could cause undefined behavior
            if (isDisruptionBetweenInstr(instr, store))
                continue;

            // To make sure the destination pointer is actually valid at the time we call the
            // PIM instruction, we insert the PIM instruction at the location of the store. This
            // is a much simpler alternative to moving the instructions that calculate the destination
            // pointer before the PIM instruction. But for this to be legal, we have to check that
            // nothing uses the result or changes the arguments between the original instruction and
            // the store instruction we want to use.
            if (isValueUsedBetweenInstr(instr, store, instr))
                continue;

            // Mark the old store for deletion
            this->instrDelList.emplace(store);

            *pos = store;
            return store->getOperand(1);
        }

        // If we didn't find a store we can use, we have to store the result
        // in temporary memory
        *pos = instr;
        return dyn_cast<Instruction>(createPIMMalloc(elementType));
    }

    static bool isDisruptionBetweenInstr(Instruction *start, Instruction *end)
    {
        // Iterate until we find the end instruction or the end of the block
        Instruction *next = start;
        while ((next = next->getNextNonDebugInstruction()))
        {
            // Check if we reached the end instruction
            if (next == end)
                return false;

            // Check if we find a disrupting instruction
            if (dyn_cast<StoreInst>(next)
                || dyn_cast<BranchInst>(next)
                || dyn_cast<CallInst>(next))
                return true;
        }
        // Reaching the end of the block but not the second instruction seems disruptive
        return true;
    }

    static bool isValueUsedBetweenInstr(Instruction *start, Instruction *end, Value *value)
    {
        // Iterate until we find the end instruction or the end of the block
        Instruction *next = start;
        while ((next = next->getNextNonDebugInstruction()))
        {
            // Check if we reached the end instruction
            if (next == end)
                return false;

            // Check if we find an instruction using the value
            for(auto &op : next->operands())
                if (op.get() == value)
                    return true;
        }
        // Reaching the end of the block but not the second instruction would be bad
        return true;
    }

    DependencyGraphNode *addScalarOperandDependency(Instruction *scalarInstr, Instruction *baseInstr, Type *elementType)
    {
        struct DependencyGraphNode node = {
                .isPIMInstr = false,
                .instr = scalarInstr
        };

        // Check if scalarInstr is a load we might be able to use directly
        auto *load = dyn_cast<LoadInst>(scalarInstr);
        if (load && !isDisruptionBetweenInstr(load, baseInstr))
        {
            // We can use the load directly
            node.dest = load->getOperand(0);
        }
        else
        {
            // It's not a load or an disrupted load, so we have to allocate memory
            // to store the result of the instruction
            node.dest = createPIMMalloc(elementType);

            // NOTE: If it is a load but it's disrupted, it might be faster to do a
            // memcpy instead, especially if it's a hardware accelerated memcpy

            // Store the result of the scalar instruction to the allocated memory.
            // As we insert the instruction, LLVM will take care of freeing the memory.
            // Getting the next instruction is safe, as a store can never be the final instruction,
            // there will always have to be a terminator (e.g. a ret).
            auto *store = new StoreInst(scalarInstr, node.dest, false, Align(4));
            this->instrInsertList.emplace_back(scalarInstr->getNextNonDebugInstruction(), store);
        }

        // Add the node to the dependency graph
        this->dependencyGraph.push_back(node);
        return &this->dependencyGraph.back();
    }

    Value *createPIMMalloc(Type *elementType)
    {
        // Get the data layout of the architecture to calculate type sizes
        const DataLayout &dataLayout = this->currentModule->getDataLayout();
        Type *ptrType = dataLayout.getIntPtrType(this->currentFunction->getContext());

        // We need a separate allocation for each type for alignment and access reasons
        unsigned int t = elementType->getIntegerBitWidth() / 8 - 1;

        // We will insert the correct allocation amount later
        auto allocSizeConst = ConstantInt::get(ptrType, 0);

        // If we haven't allocated anything yet for this function, we have to create the
        // posix_memalign function first. Otherwise we reuse is and just increase the allocated amount.
        if (this->memalignAllocCount[t] == 0)
        {
            // Create a posix_memalign
            auto [loadInst, memalignInst] = createMemalignInstr(allocSizeConst,
                                                                this->currentFunction->getEntryBlock().getFirstNonPHIOrDbg(),
                                                                this->getPIMPtrTy());

            // Create and insert the corresponding free instruction
            CallInst::CreateFree(loadInst, this->currentFunction->getBasicBlockList().back().getTerminator());

            // For now we only used one allocation
            this->memalignAllocCount[t] = 1;

            // Store some of the instructions we just created because we will still need them later
            this->memalignInst[t] = memalignInst;
            this->memalignBasePtr[t] = loadInst;

            return loadInst;
        }
        else
        {
            // Calculate the offset for our pointer
            auto *offset = ConstantInt::get(ptrType, this->memalignAllocCount[t]);

            // We use getelementptr to easily get a pointer at the specified offset
            Instruction *gepInst = GetElementPtrInst::Create(this->getPIMTy(), this->memalignBasePtr[t], {offset});
            this->instrInsertList.emplace_back(this->currentFunction->getEntryBlock().getFirstNonPHIOrDbg(), gepInst);

            // Mark this instruction as transposed
            this->transposedValues.emplace(gepInst);

            // Add the memory we just allocated to the running total
            this->memalignAllocCount[t]++;

            return gepInst;
        }
    }

    std::pair<Instruction*, Instruction*> createMemalignInstr(Value *allocSize, Instruction *insertBefore, Type *returnType)
    {
        // Get the data layout of the architecture to calculate type sizes
        const DataLayout &dataLayout = this->currentModule->getDataLayout();

        // Get common types
        Type *int32ty = IntegerType::getInt32Ty(this->currentFunction->getContext());
        Type *int8ptr = IntegerType::getInt8PtrTy(this->currentFunction->getContext());
        Type *ptrType = dataLayout.getIntPtrType(this->currentFunction->getContext());
        Instruction *nullInsert = nullptr;

        // Create a pointer to hold the result
        Instruction *allocaInstr = new AllocaInst(int8ptr, this->currentFunction->getAddressSpace(),
                                                  ConstantInt::get(int32ty, 1), Align(4));

        // Find the posix_memalign function
        auto memalignFunc = this->currentModule->getOrInsertFunction("posix_memalign", int32ty,
                                                                     int8ptr->getPointerTo(), ptrType, ptrType);

        // Create a constant for the allocation
        auto alignConst = ConstantInt::get(ptrType, PIM_VECTOR_SIZE);

        // Create the call to the posix_memalign function
        Instruction *memalignInstr = CallInst::Create(memalignFunc,
                                                      {allocaInstr, alignConst, allocSize});
        memalignInstr->setDebugLoc(insertBefore->getDebugLoc());

        // Cast the resulting pointer to the appropriate vector type
        Instruction *bitcastInst = new BitCastInst(allocaInstr, returnType->getPointerTo(), "",
                                                   nullInsert);

        // Create a load instruction to get the result
        Instruction *loadInst = new LoadInst(returnType, bitcastInst, "", false, Align(4));

        // Insert the instructions in the correct order
        this->instrInsertList.emplace_back(insertBefore, allocaInstr);
        this->instrInsertList.emplace_back(insertBefore, memalignInstr);
        this->instrInsertList.emplace_back(insertBefore, bitcastInst);
        this->instrInsertList.emplace_back(insertBefore, loadInst);

        // Transpose the memory right away as we definitely use it for PIM instructions
        addTranspInstr(loadInst, insertBefore);

        return std::make_pair(loadInst, memalignInstr);
    }

    void resolveDependencyNode(DependencyGraphNode &node)
    {
        if (node.isPIMInstr)
        {
            // Convert the opcode to a PIM instruction type
            enum PIMInstrType instrType;
            switch (node.instr->getOpcode())
            {
                case Instruction::And: instrType = PIMAnd; break;
                case Instruction::Or: instrType = PIMOr; break;
                case Instruction::Xor: instrType = PIMNot; break;
                case Instruction::Add: instrType = PIMAdd; break;
                case Instruction::Sub: instrType = PIMSub; break;
                case Instruction::Mul: instrType = PIMMul; break;
                case Instruction::SDiv: instrType = PIMDiv; break;
                case Instruction::UDiv: instrType = PIMDiv; break;
                default: std::cerr << "Invalid PIM instruction" << std::endl; return; // Should never happen
            }

            // Mark the PIM instruction as used
            this->PIMInstrUsed[instrType] = true;

            // Build the new call to the PIM function
            std::vector<Value*> args;
            args.push_back(node.dest);
            args.push_back(node.op1->dest);
            if (node.op2 != nullptr)
                args.push_back(node.op2->dest);

            CallInst *call = CallInst::Create(this->PIMFunc[instrType], args);

            // Replace the instruction
            this->instrInsertList.emplace_back(node.pos, call);
            this->instrDelList.emplace(node.instr);

            // We don't need the result loaded anymore at all, if all uses are either other
            // PIM functions or the store/bitcast that we used for the destination
            for (auto &use : node.instr->uses())
            {
                if (!isInstrPIMOrDeleted(dyn_cast<Instruction>(use.getUser())))
                {
                    // We have to still load the result of5 the PIM instruction back to the CPU
                    auto *load = new LoadInst(node.instr->getType(), node.dest, "",
                                              node.instr->getNextNonDebugInstruction());
                    this->instrReplaceUsesList.emplace_back(node.instr, load);
                    break;
                }
            }
        }
        else if (auto *load = dyn_cast<LoadInst>(node.instr))
        {
            // If all uses of the load are either PIM instructions or deleted, we can
            // delete the load as well
            for (auto &use : load->uses())
            {
                if (!isInstrPIMOrDeleted(dyn_cast<Instruction>(use.getUser())))
                    return;
            }
            this->instrDelList.emplace(load);
        }
    }

    // True if the result is not needed anymore, false otherwise
    bool isInstrPIMOrDeleted(Instruction *instr)
    {
        // If it isn't even an instruction, we should probably not consider it deleted
        if (instr == nullptr)
            return false;

        // Check whether the instruction is a PIM function
        for (auto node : this->dependencyGraph)
            if (node.instr == instr)
                return true;

        // Check whether the instruction is deleted (i.e. the store/bitcast)
        for (auto element : this->instrDelList)
            if (element == instr)
                return true;

        return false;
    }

    bool removeUnusedPIMFunction(Module &module, PIMInstrType instrType)
    {
        // If the function was never used, we might as well remove it again
        if (!this->PIMInstrUsed[instrType])
        {
            this->PIMFunc[instrType]->removeFromParent();
            return true;
        }

        return false;
    }

    VectorType *getPIMTy()
    {
        return VectorType::get(Type::getInt32Ty(this->currentFunction->getContext()),
                                                PIM_VECTOR_SIZE, false);
    }

    PointerType *getPIMPtrTy()
    {
        return PointerType::get(
                   VectorType::get(
                       Type::getInt32Ty(this->currentFunction->getContext()),
                       PIM_VECTOR_SIZE, false),
                   this->currentFunction->getAddressSpace());
    }
};

}

char PIMVectorizationPass::ID = 0;

static RegisterPass<PIMVectorizationPass> X("pim", "PIM optimizations for gem5",
                                            false, false);

static RegisterStandardPasses Y(
        PassManagerBuilder::EP_OptimizerLast,
        [](const PassManagerBuilder &builder,
           legacy::PassManagerBase &PM) { PM.add(new PIMVectorizationPass()); });
