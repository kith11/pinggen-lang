#include "pinggen/llvm_ir.hpp"

#include <sstream>

namespace pinggen {

namespace {

EnumDecl builtin_fs_result_enum() {
    EnumDecl decl;
    decl.location = {1, 1};
    decl.name = "FsResult";

    EnumVariant ok;
    ok.location = {1, 1};
    ok.name = "Ok";
    ok.payload_type = Type::string_type();
    decl.variants.push_back(std::move(ok));

    EnumVariant err;
    err.location = {1, 1};
    err.name = "Err";
    err.payload_type = Type::string_type();
    decl.variants.push_back(std::move(err));

    return decl;
}

EnumDecl builtin_fs_write_result_enum() {
    EnumDecl decl;
    decl.location = {1, 1};
    decl.name = "FsWriteResult";

    EnumVariant ok;
    ok.location = {1, 1};
    ok.name = "Ok";
    decl.variants.push_back(std::move(ok));

    EnumVariant err;
    err.location = {1, 1};
    err.name = "Err";
    err.payload_type = Type::string_type();
    decl.variants.push_back(std::move(err));

    return decl;
}

bool program_imports_std_item(const Program& program, const std::string& item) {
    for (const auto& import_decl : program.imports) {
        if (import_decl.kind != ImportKind::Std || import_decl.module_name != "std") {
            continue;
        }
        for (const auto& imported_item : import_decl.items) {
            if (imported_item == item) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

std::string LLVMIRGenerator::lowered_function_name(const FunctionDecl& function) {
    if (!function.is_method()) {
        return function.name;
    }
    return function.impl_target + "__" + function.name;
}

Type LLVMIRGenerator::normalize_type(const Type& type) const {
    if (type.kind == TypeKind::Array) {
        if (!type.element_type) {
            return type;
        }
        return Type::array_type(normalize_type(*type.element_type), type.array_size);
    }
    if (type.kind == TypeKind::Tuple) {
        std::vector<Type> elements;
        elements.reserve(type.tuple_elements.size());
        for (const auto& element : type.tuple_elements) {
            elements.push_back(normalize_type(element));
        }
        return Type::tuple_type(std::move(elements));
    }
    if (type.kind == TypeKind::Struct && enums_.contains(type.name)) {
        return Type::enum_type(type.name);
    }
    return type;
}

bool LLVMIRGenerator::enum_has_payload(const std::string& enum_name) const {
    const auto it = enums_.find(enum_name);
    if (it == enums_.end()) {
        return false;
    }
    for (const auto& variant : it->second.variants) {
        if (variant.payload_type.has_value()) {
            return true;
        }
    }
    return false;
}

std::optional<Type> LLVMIRGenerator::enum_payload_type(const std::string& enum_name, const std::string& variant) const {
    const auto enum_it = enums_.find(enum_name);
    if (enum_it == enums_.end()) {
        return std::nullopt;
    }
    const auto variant_it = enum_variant_indices_.find(enum_name);
    if (variant_it == enum_variant_indices_.end()) {
        return std::nullopt;
    }
    const auto ordinal_it = variant_it->second.find(variant);
    if (ordinal_it == variant_it->second.end()) {
        return std::nullopt;
    }
    const auto& decl_variant = enum_it->second.variants[ordinal_it->second];
    if (!decl_variant.payload_type.has_value()) {
        return std::nullopt;
    }
    return normalize_type(*decl_variant.payload_type);
}

std::string LLVMIRGenerator::generate(const Program& program) {
    globals_.clear();
    functions_.clear();
    body_.clear();
    enums_.clear();
    enum_variant_indices_.clear();
    enum_payload_field_indices_.clear();
    structs_.clear();
    struct_field_indices_.clear();
    variables_.clear();
    variable_types_.clear();
    function_return_types_.clear();
    mutating_methods_.clear();
    con_safe_functions_.clear();
    register_counter_ = 0;
    string_counter_ = 0;
    label_counter_ = 0;
    con_counter_ = 0;
    uses_con_runtime_ = false;
    extra_type_defs_.clear();
    helper_functions_.clear();
    current_function_name_.clear();
    current_return_type_ = Type::void_type();
    break_labels_.clear();
    continue_labels_.clear();

    globals_ += "@.fmt.int = private unnamed_addr constant [6 x i8] c\"%lld\\0A\\00\"\n";
    globals_ += "@.fmt.str = private unnamed_addr constant [4 x i8] c\"%s\\0A\\00\"\n";
    const bool has_fs_import = program_imports_std_item(program, "fs");
    if (has_fs_import) {
        globals_ += "@.fs.mode.rb = private unnamed_addr constant [3 x i8] c\"rb\\00\"\n";
        globals_ += "@.fs.mode.wb = private unnamed_addr constant [3 x i8] c\"wb\\00\"\n";
        globals_ += "@.fs.err.open = private unnamed_addr constant [20 x i8] c\"failed to open file\\00\"\n";
        globals_ += "@.fs.err.read = private unnamed_addr constant [20 x i8] c\"failed to read file\\00\"\n";
        globals_ += "@.fs.err.write = private unnamed_addr constant [21 x i8] c\"failed to write file\\00\"\n";
    }

    if (has_fs_import) {
        for (const EnumDecl builtin : {builtin_fs_result_enum(), builtin_fs_write_result_enum()}) {
            enums_[builtin.name] = builtin;
            std::size_t payload_field_index = 1;
            for (std::size_t i = 0; i < builtin.variants.size(); ++i) {
                enum_variant_indices_[builtin.name][builtin.variants[i].name] = i;
                if (builtin.variants[i].payload_type.has_value()) {
                    enum_payload_field_indices_[builtin.name][builtin.variants[i].name] = payload_field_index++;
                }
            }
        }
    }
    for (const auto& decl : program.enums) {
        enums_[decl.name] = decl;
        std::size_t payload_field_index = 1;
        for (std::size_t i = 0; i < decl.variants.size(); ++i) {
            enum_variant_indices_[decl.name][decl.variants[i].name] = i;
            if (decl.variants[i].payload_type.has_value()) {
                enum_payload_field_indices_[decl.name][decl.variants[i].name] = payload_field_index++;
            }
        }
    }

    std::ostringstream type_defs;
    for (const auto& [enum_name, decl] : enums_) {
        if (!enum_has_payload(enum_name)) {
            continue;
        }
        type_defs << "%enum." << enum_name << " = type { i64";
        for (const auto& variant : decl.variants) {
            if (!variant.payload_type.has_value()) {
                continue;
            }
            type_defs << ", " << llvm_type(normalize_type(*variant.payload_type));
        }
        type_defs << " }\n";
    }
    for (const auto& decl : program.structs) {
        structs_[decl.name] = decl;
        for (std::size_t i = 0; i < decl.fields.size(); ++i) {
            struct_field_indices_[decl.name][decl.fields[i].name] = i;
        }
        type_defs << "%struct." << decl.name << " = type { ";
        for (std::size_t i = 0; i < decl.fields.size(); ++i) {
            if (i != 0) {
                type_defs << ", ";
            }
            type_defs << llvm_type(normalize_type(decl.fields[i].type));
        }
        type_defs << " }\n";
    }

    for (const auto& function : program.functions) {
        function_return_types_[lowered_function_name(function)] =
            normalize_type(function.return_type) == Type::void_type() && function.name == "main" ? Type::int_type()
                                                                                                 : normalize_type(function.return_type);
        mutating_methods_[lowered_function_name(function)] = function.is_mutating_method();
        con_safe_functions_[lowered_function_name(function)] = function.is_con_safe;
    }

    for (const auto& function : program.functions) {
        reset_function_state();
        current_function_name_ = lowered_function_name(function);
        current_return_type_ = function_return_types_.at(current_function_name_);

        std::ostringstream signature;
        signature << "define " << (function.name == "main" ? "i32" : llvm_type(current_return_type_)) << " @" << current_function_name_
                  << "(";
        for (std::size_t i = 0; i < function.params.size(); ++i) {
            if (i != 0) {
                signature << ", ";
            }
            const Type param_type = normalize_type(function.params[i].type);
            if (function.is_mutating_method() && i == 0) {
                signature << "ptr %arg" << i;
            } else {
                signature << llvm_type(param_type) << " %arg" << i;
            }
        }
        signature << ") {\n";

        for (std::size_t i = 0; i < function.params.size(); ++i) {
            const auto& param = function.params[i];
            const Type param_type = normalize_type(param.type);
            if (function.is_mutating_method() && i == 0) {
                variables_[param.name] = "%arg" + std::to_string(i);
                variable_types_[param.name] = param_type;
                continue;
            }
            const std::string storage = next_register();
            body_ += "  " + storage + " = alloca " + llvm_type(param_type) + "\n";
            body_ += "  store " + llvm_type(param_type) + " %arg" + std::to_string(i) + ", ptr " + storage + "\n";
            variables_[param.name] = storage;
            variable_types_[param.name] = param_type;
        }

        const bool has_return = emit_block(function.body);
        if (!has_return) {
            if (function.name == "main") {
                body_ += "  ret i32 0\n";
            } else if (current_return_type_ == Type::void_type()) {
                body_ += "  ret void\n";
            }
        }

        signature << body_;
        signature << "}\n\n";
        functions_ += signature.str();
    }
    std::ostringstream out;
    out << "; generated by pinggen\n";
    out << "declare i32 @printf(ptr, ...)\n\n";
    out << "declare void @exit(i32)\n";
    out << "declare i64 @strlen(ptr)\n";
    out << "declare ptr @malloc(i64)\n";
    out << "declare ptr @memcpy(ptr, ptr, i64)\n";
    if (uses_con_runtime_) {
        out << "declare ptr @pinggen_con_group_create(i64)\n";
        out << "declare void @pinggen_con_spawn(ptr, ptr, ptr)\n";
        out << "declare void @pinggen_con_wait(ptr)\n";
    }
    out << "\n";
    if (has_fs_import) {
        out << "declare ptr @fopen(ptr, ptr)\n";
        out << "declare i32 @fclose(ptr)\n";
        out << "declare i32 @fseek(ptr, i64, i32)\n";
        out << "declare i64 @ftell(ptr)\n";
        out << "declare i64 @fread(ptr, i64, i64, ptr)\n";
        out << "declare i64 @fwrite(ptr, i64, i64, ptr)\n\n";
    }
    if (!program.structs.empty() || !enum_payload_field_indices_.empty() || !extra_type_defs_.empty()) {
        out << type_defs.str();
        out << extra_type_defs_;
        out << "\n";
    }
    out << emit_concat_helper();
    out << emit_bounds_abort_helper();
    if (has_fs_import) {
        out << emit_fs_read_helper();
        out << emit_fs_write_helper();
        out << emit_fs_exists_helper();
    }
    out << helper_functions_;
    out << globals_ << "\n";
    out << functions_;
    return out.str();
}

std::string LLVMIRGenerator::emit_concat_helper() const {
    return
        "define ptr @pinggen_concat(ptr %lhs, ptr %rhs) {\n"
        "  %1 = call i64 @strlen(ptr %lhs)\n"
        "  %2 = call i64 @strlen(ptr %rhs)\n"
        "  %3 = add i64 %1, %2\n"
        "  %4 = add i64 %3, 1\n"
        "  %5 = call ptr @malloc(i64 %4)\n"
        "  %6 = call ptr @memcpy(ptr %5, ptr %lhs, i64 %1)\n"
        "  %7 = getelementptr inbounds i8, ptr %5, i64 %1\n"
        "  %8 = call ptr @memcpy(ptr %7, ptr %rhs, i64 %2)\n"
        "  %9 = getelementptr inbounds i8, ptr %5, i64 %3\n"
        "  store i8 0, ptr %9\n"
        "  ret ptr %5\n"
        "}\n\n";
}

std::string LLVMIRGenerator::emit_bounds_abort_helper() const {
    return
        "define void @pinggen_bounds_abort() {\n"
        "  call void @exit(i32 1)\n"
        "  unreachable\n"
        "}\n\n";
}

std::string LLVMIRGenerator::emit_fs_read_helper() const {
    return
        "define %enum.FsResult @pinggen_fs_read_to_string(ptr %path) {\n"
        "entry:\n"
        "  %mode = getelementptr inbounds [3 x i8], ptr @.fs.mode.rb, i64 0, i64 0\n"
        "  %file = call ptr @fopen(ptr %path, ptr %mode)\n"
        "  %file_ok = icmp ne ptr %file, null\n"
        "  br i1 %file_ok, label %seek_end, label %open_fail\n"
        "open_fail:\n"
        "  %open_msg = getelementptr inbounds [20 x i8], ptr @.fs.err.open, i64 0, i64 0\n"
        "  %open_res0 = insertvalue %enum.FsResult zeroinitializer, i64 1, 0\n"
        "  %open_res1 = insertvalue %enum.FsResult %open_res0, ptr %open_msg, 2\n"
        "  ret %enum.FsResult %open_res1\n"
        "seek_end:\n"
        "  %seek_end_ok = call i32 @fseek(ptr %file, i64 0, i32 2)\n"
        "  %seek_end_cmp = icmp eq i32 %seek_end_ok, 0\n"
        "  br i1 %seek_end_cmp, label %tell_size, label %read_fail_close\n"
        "tell_size:\n"
        "  %size = call i64 @ftell(ptr %file)\n"
        "  %size_ok = icmp sge i64 %size, 0\n"
        "  br i1 %size_ok, label %seek_start, label %read_fail_close\n"
        "seek_start:\n"
        "  %seek_start_ok = call i32 @fseek(ptr %file, i64 0, i32 0)\n"
        "  %seek_start_cmp = icmp eq i32 %seek_start_ok, 0\n"
        "  br i1 %seek_start_cmp, label %alloc_buf, label %read_fail_close\n"
        "alloc_buf:\n"
        "  %alloc_size = add i64 %size, 1\n"
        "  %buffer = call ptr @malloc(i64 %alloc_size)\n"
        "  %read_count = call i64 @fread(ptr %buffer, i64 1, i64 %size, ptr %file)\n"
        "  %close_ok = call i32 @fclose(ptr %file)\n"
        "  %read_exact = icmp eq i64 %read_count, %size\n"
        "  br i1 %read_exact, label %terminate, label %read_fail\n"
        "terminate:\n"
        "  %end_ptr = getelementptr inbounds i8, ptr %buffer, i64 %size\n"
        "  store i8 0, ptr %end_ptr\n"
        "  %ok_res0 = insertvalue %enum.FsResult zeroinitializer, i64 0, 0\n"
        "  %ok_res1 = insertvalue %enum.FsResult %ok_res0, ptr %buffer, 1\n"
        "  ret %enum.FsResult %ok_res1\n"
        "read_fail_close:\n"
        "  %close_fail = call i32 @fclose(ptr %file)\n"
        "  br label %read_fail\n"
        "read_fail:\n"
        "  %read_msg = getelementptr inbounds [20 x i8], ptr @.fs.err.read, i64 0, i64 0\n"
        "  %read_res0 = insertvalue %enum.FsResult zeroinitializer, i64 1, 0\n"
        "  %read_res1 = insertvalue %enum.FsResult %read_res0, ptr %read_msg, 2\n"
        "  ret %enum.FsResult %read_res1\n"
        "}\n\n";
}

std::string LLVMIRGenerator::emit_fs_write_helper() const {
    return
        "define %enum.FsWriteResult @pinggen_fs_write_string(ptr %path, ptr %contents) {\n"
        "entry:\n"
        "  %mode = getelementptr inbounds [3 x i8], ptr @.fs.mode.wb, i64 0, i64 0\n"
        "  %file = call ptr @fopen(ptr %path, ptr %mode)\n"
        "  %file_ok = icmp ne ptr %file, null\n"
        "  br i1 %file_ok, label %write_setup, label %open_fail\n"
        "open_fail:\n"
        "  %open_msg = getelementptr inbounds [20 x i8], ptr @.fs.err.open, i64 0, i64 0\n"
        "  %open_res0 = insertvalue %enum.FsWriteResult zeroinitializer, i64 1, 0\n"
        "  %open_res1 = insertvalue %enum.FsWriteResult %open_res0, ptr %open_msg, 1\n"
        "  ret %enum.FsWriteResult %open_res1\n"
        "write_setup:\n"
        "  %len = call i64 @strlen(ptr %contents)\n"
        "  %written = call i64 @fwrite(ptr %contents, i64 1, i64 %len, ptr %file)\n"
        "  %close_status = call i32 @fclose(ptr %file)\n"
        "  %write_exact = icmp eq i64 %written, %len\n"
        "  %close_ok = icmp eq i32 %close_status, 0\n"
        "  %all_ok = and i1 %write_exact, %close_ok\n"
        "  br i1 %all_ok, label %ok, label %write_fail\n"
        "ok:\n"
        "  %ok_res = insertvalue %enum.FsWriteResult zeroinitializer, i64 0, 0\n"
        "  ret %enum.FsWriteResult %ok_res\n"
        "write_fail:\n"
        "  %write_msg = getelementptr inbounds [21 x i8], ptr @.fs.err.write, i64 0, i64 0\n"
        "  %write_res0 = insertvalue %enum.FsWriteResult zeroinitializer, i64 1, 0\n"
        "  %write_res1 = insertvalue %enum.FsWriteResult %write_res0, ptr %write_msg, 1\n"
        "  ret %enum.FsWriteResult %write_res1\n"
        "}\n\n";
}

std::string LLVMIRGenerator::emit_fs_exists_helper() const {
    return
        "define i1 @pinggen_fs_exists(ptr %path) {\n"
        "entry:\n"
        "  %mode = getelementptr inbounds [3 x i8], ptr @.fs.mode.rb, i64 0, i64 0\n"
        "  %file = call ptr @fopen(ptr %path, ptr %mode)\n"
        "  %exists = icmp ne ptr %file, null\n"
        "  br i1 %exists, label %close_file, label %done\n"
        "close_file:\n"
        "  %close_status = call i32 @fclose(ptr %file)\n"
        "  br label %done\n"
        "done:\n"
        "  ret i1 %exists\n"
        "}\n\n";
}

void LLVMIRGenerator::emit_bounds_check(const std::string& index_ir, std::size_t size) {
    const std::string nonnegative = next_register();
    const std::string within = next_register();
    const std::string ok = next_register();
    const std::string fail_label = next_label("bounds_fail");
    const std::string ok_label = next_label("bounds_ok");
    body_ += "  " + nonnegative + " = icmp sge i64 " + index_ir + ", 0\n";
    body_ += "  " + within + " = icmp slt i64 " + index_ir + ", " + std::to_string(size) + "\n";
    body_ += "  " + ok + " = and i1 " + nonnegative + ", " + within + "\n";
    body_ += "  br i1 " + ok + ", label %" + ok_label + ", label %" + fail_label + "\n";
    body_ += fail_label + ":\n";
    body_ += "  call void @pinggen_bounds_abort()\n";
    body_ += "  unreachable\n";
    body_ += ok_label + ":\n";
}

std::string LLVMIRGenerator::emit_string_constant(const std::string& value) {
    const std::string name = next_string_name();
    const std::size_t byte_len = value.size() + 1;
    std::ostringstream out;
    out << "@" << name << " = private unnamed_addr constant [" << byte_len << " x i8] c\"" << escape_bytes(value)
        << "\\00\"\n";
    globals_ += out.str();
    return name + "|" + std::to_string(byte_len);
}

LLVMIRGenerator::LoweredConTask LLVMIRGenerator::lower_con_task(const Expr& item, std::size_t source_index) {
    LoweredConTask task;
    task.source_index = source_index;

    if (const auto* call = dynamic_cast<const CallExpr*>(&item)) {
        task.kind = ConTaskKind::Function;
        task.lowered_callee = call->callee;
        task.result_type = function_return_types_.at(call->callee);
        for (const auto& arg : call->args) {
            TypedIRValue value = emit_expr(*arg);
            task.capture_types.push_back(value.type);
            task.capture_is_receiver.push_back(false);
            task.captures.push_back(std::move(value));
        }
        return task;
    }

    const auto* method = dynamic_cast<const MethodCallExpr*>(&item);
    task.kind = ConTaskKind::Method;
    const TypedIRValue object_value = emit_expr(*method->object);
    task.lowered_callee = object_value.type.name + "__" + method->method;
    task.result_type = function_return_types_.at(task.lowered_callee);
    task.capture_types.push_back(object_value.type);
    task.capture_is_receiver.push_back(true);
    task.captures.push_back(object_value);
    for (const auto& arg : method->args) {
        TypedIRValue value = emit_expr(*arg);
        task.capture_types.push_back(value.type);
        task.capture_is_receiver.push_back(false);
        task.captures.push_back(std::move(value));
    }
    return task;
}

LLVMIRGenerator::LoweredConExpr LLVMIRGenerator::lower_con_expr(const ConExpr& expr) {
    LoweredConExpr lowering;
    lowering.sync_region_id = ++con_counter_;
    lowering.tasks.reserve(expr.items.size());

    for (std::size_t i = 0; i < expr.items.size(); ++i) {
        LoweredConTask task = lower_con_task(*expr.items[i], i);
        task.context_type_name = emit_con_context_type(lowering, task);
        task.task_name = emit_con_task_function(lowering, task);
        if (task.result_type == Type::void_type()) {
            lowering.tasks.push_back(std::move(task));
            continue;
        }
        lowering.all_void = false;
        lowering.result_types.push_back(task.result_type);
        lowering.tasks.push_back(std::move(task));
    }

    return lowering;
}

std::string LLVMIRGenerator::emit_con_context_type(const LoweredConExpr& lowering, const LoweredConTask& task) {
    const std::string name =
        "%con.sync." + std::to_string(lowering.sync_region_id) + ".ctx." + std::to_string(task.source_index);
    extra_type_defs_ += name + " = type { ";
    for (std::size_t i = 0; i < task.capture_types.size(); ++i) {
        if (i != 0) {
            extra_type_defs_ += ", ";
        }
        extra_type_defs_ += llvm_type(task.capture_types[i]);
    }
    if (task.result_type != Type::void_type()) {
        if (!task.capture_types.empty()) {
            extra_type_defs_ += ", ";
        }
        extra_type_defs_ += "ptr";
    }
    extra_type_defs_ += " }\n";
    return name;
}

std::string LLVMIRGenerator::emit_con_task_function(const LoweredConExpr& lowering, const LoweredConTask& task) {
    const std::string task_name =
        "pinggen_con_task_" + std::to_string(lowering.sync_region_id) + "_" + std::to_string(task.source_index);
    std::string fn;
    fn += "define void @" + task_name + "(ptr %ctx) {\n";
    fn += "entry:\n";

    std::vector<std::string> loaded_values;
    for (std::size_t i = 0; i < task.capture_types.size(); ++i) {
        const std::string field_ptr = "%ctx_field_ptr_" + std::to_string(i + 1);
        const std::string field_value = "%ctx_field_value_" + std::to_string(i + 1);
        fn += "  " + field_ptr + " = getelementptr inbounds " + task.context_type_name + ", ptr %ctx, i32 0, i32 " +
              std::to_string(i) + "\n";
        fn += "  " + field_value + " = load " + llvm_type(task.capture_types[i]) + ", ptr " + field_ptr + "\n";
        loaded_values.push_back(field_value);
    }

    std::string call_ir = "@" + task.lowered_callee + "(";
    for (std::size_t i = 0; i < task.capture_types.size(); ++i) {
        if (i != 0) {
            call_ir += ", ";
        }
        call_ir += llvm_type(task.capture_types[i]) + " " + loaded_values[i];
    }
    call_ir += ")";

    if (task.result_type == Type::void_type()) {
        fn += "  call void " + call_ir + "\n";
    } else {
        fn += "  %task_result = call " + llvm_type(task.result_type) + " " + call_ir + "\n";
        const std::string result_ptr = "%result_ptr";
        fn += "  " + result_ptr + " = getelementptr inbounds " + task.context_type_name + ", ptr %ctx, i32 0, i32 " +
              std::to_string(task.capture_types.size()) + "\n";
        fn += "  %result_slot = load ptr, ptr " + result_ptr + "\n";
        fn += "  store " + llvm_type(task.result_type) + " %task_result, ptr %result_slot\n";
    }
    fn += "  ret void\n";
    fn += "}\n\n";
    helper_functions_ += fn;
    return task_name;
}

TypedIRValue LLVMIRGenerator::emit_lowered_con_expr(const LoweredConExpr& lowering) {
    const std::string group_reg = next_register();
    body_ += "  " + group_reg + " = call ptr @pinggen_con_group_create(i64 " + std::to_string(lowering.tasks.size()) + ")\n";

    std::vector<std::string> result_slots;
    for (const auto& task : lowering.tasks) {
        const bool has_result_slot = task.result_type != Type::void_type();
        const std::string result_slot = has_result_slot ? next_register() : "";
        if (has_result_slot) {
            body_ += "  " + result_slot + " = alloca " + llvm_type(task.result_type) + "\n";
            result_slots.push_back(result_slot);
        }

        const std::string ctx_storage = next_register();
        body_ += "  " + ctx_storage + " = alloca " + task.context_type_name + "\n";
        for (std::size_t capture_index = 0; capture_index < task.captures.size(); ++capture_index) {
            const std::string field_ptr = next_register();
            body_ += "  " + field_ptr + " = getelementptr inbounds " + task.context_type_name + ", ptr " + ctx_storage +
                     ", i32 0, i32 " + std::to_string(capture_index) + "\n";
            body_ += "  store " + llvm_type(task.captures[capture_index].type) + " " + task.captures[capture_index].ir +
                     ", ptr " + field_ptr + "\n";
        }
        if (has_result_slot) {
            const std::string field_ptr = next_register();
            body_ += "  " + field_ptr + " = getelementptr inbounds " + task.context_type_name + ", ptr " + ctx_storage +
                     ", i32 0, i32 " + std::to_string(task.captures.size()) + "\n";
            body_ += "  store ptr " + result_slot + ", ptr " + field_ptr + "\n";
        }
        body_ += "  call void @pinggen_con_spawn(ptr " + group_reg + ", ptr @" + task.task_name + ", ptr " + ctx_storage + ")\n";
    }

    body_ += "  call void @pinggen_con_wait(ptr " + group_reg + ")\n";
    if (lowering.all_void) {
        return {"0", Type::void_type()};
    }

    const Type tuple_type = Type::tuple_type(lowering.result_types);
    const std::string tuple_storage = next_register();
    body_ += "  " + tuple_storage + " = alloca " + llvm_type(tuple_type) + "\n";
    for (std::size_t i = 0; i < lowering.result_types.size(); ++i) {
        const std::string value_reg = next_register();
        const std::string field_ptr = next_register();
        body_ += "  " + value_reg + " = load " + llvm_type(lowering.result_types[i]) + ", ptr " + result_slots[i] + "\n";
        body_ += "  " + field_ptr + " = getelementptr inbounds " + llvm_type(tuple_type) + ", ptr " + tuple_storage +
                 ", i32 0, i32 " + std::to_string(i) + "\n";
        body_ += "  store " + llvm_type(lowering.result_types[i]) + " " + value_reg + ", ptr " + field_ptr + "\n";
    }
    const std::string tuple_reg = next_register();
    body_ += "  " + tuple_reg + " = load " + llvm_type(tuple_type) + ", ptr " + tuple_storage + "\n";
    return {tuple_reg, tuple_type};
}

TypedIRValue LLVMIRGenerator::emit_con_expr(const ConExpr& expr) {
    uses_con_runtime_ = true;
    const LoweredConExpr lowering = lower_con_expr(expr);
    return emit_lowered_con_expr(lowering);
}

AddressValue LLVMIRGenerator::emit_address(const Expr& expr) {
    if (const auto* node = dynamic_cast<const VariableExpr*>(&expr)) {
        return {variables_.at(node->name), variable_types_.at(node->name)};
    }
    if (const auto* node = dynamic_cast<const FieldAccessExpr*>(&expr)) {
        const AddressValue base = emit_address(*node->object);
        const auto field_index = struct_field_indices_.at(base.type.name).at(node->field);
        const Type field_type = normalize_type(structs_.at(base.type.name).fields[field_index].type);
        const std::string reg = next_register();
        body_ += "  " + reg + " = getelementptr inbounds " + llvm_type(base.type) + ", ptr " + base.address + ", i32 0, i32 " +
                 std::to_string(field_index) + "\n";
        return {reg, field_type};
    }
    if (const auto* node = dynamic_cast<const IndexExpr*>(&expr)) {
        const AddressValue base = emit_address(*node->object);
        const TypedIRValue index = emit_expr(*node->index);
        emit_bounds_check(index.ir, base.type.array_size);
        const std::string reg = next_register();
        body_ += "  " + reg + " = getelementptr inbounds " + llvm_type(base.type) + ", ptr " + base.address +
                 ", i32 0, i64 " + index.ir + "\n";
        return {reg, *base.type.element_type};
    }
    const TypedIRValue value = emit_expr(expr);
    const std::string storage = next_register();
    body_ += "  " + storage + " = alloca " + llvm_type(value.type) + "\n";
    body_ += "  store " + llvm_type(value.type) + " " + value.ir + ", ptr " + storage + "\n";
    return {storage, value.type};
}

TypedIRValue LLVMIRGenerator::emit_expr(const Expr& expr) {
    if (const auto* node = dynamic_cast<const IntegerExpr*>(&expr)) {
        return {std::to_string(node->value), Type::int_type()};
    }
    if (const auto* node = dynamic_cast<const BoolExpr*>(&expr)) {
        return {node->value ? "1" : "0", Type::bool_type()};
    }
    if (const auto* node = dynamic_cast<const StringExpr*>(&expr)) {
        const std::string constant = emit_string_constant(node->value);
        const auto sep = constant.find('|');
        const std::string global_name = constant.substr(0, sep);
        const std::size_t len = static_cast<std::size_t>(std::stoull(constant.substr(sep + 1)));
        const std::string reg = next_register();
        body_ += "  " + reg + " = getelementptr inbounds [" + std::to_string(len) + " x i8], ptr @" + global_name +
                 ", i64 0, i64 0\n";
        return {reg, Type::string_type()};
    }
    if (const auto* node = dynamic_cast<const TupleExpr*>(&expr)) {
        std::vector<TypedIRValue> elements;
        std::vector<Type> element_types;
        elements.reserve(node->elements.size());
        element_types.reserve(node->elements.size());
        for (const auto& element : node->elements) {
            TypedIRValue value = emit_expr(*element);
            elements.push_back(value);
            element_types.push_back(value.type);
        }
        const Type tuple_type = Type::tuple_type(element_types);
        const std::string storage = next_register();
        body_ += "  " + storage + " = alloca " + llvm_type(tuple_type) + "\n";
        for (std::size_t i = 0; i < elements.size(); ++i) {
            const std::string field_ptr = next_register();
            body_ += "  " + field_ptr + " = getelementptr inbounds " + llvm_type(tuple_type) + ", ptr " + storage +
                     ", i32 0, i32 " + std::to_string(i) + "\n";
            body_ += "  store " + llvm_type(elements[i].type) + " " + elements[i].ir + ", ptr " + field_ptr + "\n";
        }
        const std::string reg = next_register();
        body_ += "  " + reg + " = load " + llvm_type(tuple_type) + ", ptr " + storage + "\n";
        return {reg, tuple_type};
    }
    if (const auto* node = dynamic_cast<const EnumValueExpr*>(&expr)) {
        const std::size_t ordinal = enum_variant_indices_.at(node->enum_name).at(node->variant);
        const Type enum_type = Type::enum_type(node->enum_name);
        if (!enum_has_payload(node->enum_name)) {
            return {std::to_string(ordinal), enum_type};
        }
        const std::string storage = next_register();
        body_ += "  " + storage + " = alloca " + llvm_type(enum_type) + "\n";
        body_ += "  store " + llvm_type(enum_type) + " zeroinitializer, ptr " + storage + "\n";
        const std::string tag_ptr = next_register();
        body_ += "  " + tag_ptr + " = getelementptr inbounds " + llvm_type(enum_type) + ", ptr " + storage +
                 ", i32 0, i32 0\n";
        body_ += "  store i64 " + std::to_string(ordinal) + ", ptr " + tag_ptr + "\n";
        if (node->payload) {
            const Type payload_type = *enum_payload_type(node->enum_name, node->variant);
            const TypedIRValue payload_value = emit_expr(*node->payload);
            const std::size_t payload_field_index = enum_payload_field_indices_.at(node->enum_name).at(node->variant);
            const std::string payload_ptr = next_register();
            body_ += "  " + payload_ptr + " = getelementptr inbounds " + llvm_type(enum_type) + ", ptr " + storage +
                     ", i32 0, i32 " + std::to_string(payload_field_index) + "\n";
            body_ += "  store " + llvm_type(payload_type) + " " + payload_value.ir + ", ptr " + payload_ptr + "\n";
        }
        const std::string reg = next_register();
        body_ += "  " + reg + " = load " + llvm_type(enum_type) + ", ptr " + storage + "\n";
        return {reg, enum_type};
    }
    if (const auto* node = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
        const TypedIRValue first = emit_expr(*node->elements[0]);
        const Type array_type = Type::array_type(first.type, node->elements.size());
        const std::string storage = next_register();
        body_ += "  " + storage + " = alloca " + llvm_type(array_type) + "\n";
        const std::string first_ptr = next_register();
        body_ += "  " + first_ptr + " = getelementptr inbounds " + llvm_type(array_type) + ", ptr " + storage +
                 ", i32 0, i64 0\n";
        body_ += "  store " + llvm_type(first.type) + " " + first.ir + ", ptr " + first_ptr + "\n";
        for (std::size_t i = 1; i < node->elements.size(); ++i) {
            const TypedIRValue value = emit_expr(*node->elements[i]);
            const std::string element_ptr = next_register();
            body_ += "  " + element_ptr + " = getelementptr inbounds " + llvm_type(array_type) + ", ptr " + storage +
                     ", i32 0, i64 " + std::to_string(i) + "\n";
            body_ += "  store " + llvm_type(value.type) + " " + value.ir + ", ptr " + element_ptr + "\n";
        }
        const std::string reg = next_register();
        body_ += "  " + reg + " = load " + llvm_type(array_type) + ", ptr " + storage + "\n";
        return {reg, array_type};
    }
    if (const auto* node = dynamic_cast<const StructLiteralExpr*>(&expr)) {
        const Type struct_type = Type::struct_type(node->struct_name);
        const std::string storage = next_register();
        body_ += "  " + storage + " = alloca " + llvm_type(struct_type) + "\n";
        for (const auto& field : node->fields) {
            const std::size_t index = struct_field_indices_.at(node->struct_name).at(field.name);
            const Type field_type = normalize_type(structs_.at(node->struct_name).fields[index].type);
            const TypedIRValue value = emit_expr(*field.value);
            const std::string field_ptr = next_register();
            body_ += "  " + field_ptr + " = getelementptr inbounds " + llvm_type(struct_type) + ", ptr " + storage +
                     ", i32 0, i32 " + std::to_string(index) + "\n";
            body_ += "  store " + llvm_type(field_type) + " " + value.ir + ", ptr " + field_ptr + "\n";
        }
        const std::string reg = next_register();
        body_ += "  " + reg + " = load " + llvm_type(struct_type) + ", ptr " + storage + "\n";
        return {reg, struct_type};
    }
    if (dynamic_cast<const VariableExpr*>(&expr) || dynamic_cast<const FieldAccessExpr*>(&expr) ||
        dynamic_cast<const IndexExpr*>(&expr)) {
        const AddressValue address = emit_address(expr);
        const std::string reg = next_register();
        body_ += "  " + reg + " = load " + llvm_type(address.type) + ", ptr " + address.address + "\n";
        return {reg, address.type};
    }
    if (const auto* node = dynamic_cast<const UnaryExpr*>(&expr)) {
        const TypedIRValue value = emit_expr(*node->expr);
        if (node->op == "!") {
            const std::string reg = next_register();
            body_ += "  " + reg + " = xor i1 " + value.ir + ", true\n";
            return {reg, Type::bool_type()};
        }
    }
    if (const auto* node = dynamic_cast<const BinaryExpr*>(&expr)) {
        const TypedIRValue left = emit_expr(*node->left);
        const TypedIRValue right = emit_expr(*node->right);
        const std::string reg = next_register();

        if (node->op == "&&") {
            body_ += "  " + reg + " = and i1 " + left.ir + ", " + right.ir + "\n";
            return {reg, Type::bool_type()};
        }
        if (node->op == "||") {
            body_ += "  " + reg + " = or i1 " + left.ir + ", " + right.ir + "\n";
            return {reg, Type::bool_type()};
        }
        if (node->op == "==" || node->op == "!=") {
            const std::string cmp = node->op == "==" ? "eq" : "ne";
            body_ += "  " + reg + " = icmp " + cmp + " " + llvm_type(left.type) + " " + left.ir + ", " + right.ir + "\n";
            return {reg, Type::bool_type()};
        }
        if (node->op == "<" || node->op == "<=" || node->op == ">" || node->op == ">=") {
            std::string cmp;
            if (node->op == "<") cmp = "slt";
            if (node->op == "<=") cmp = "sle";
            if (node->op == ">") cmp = "sgt";
            if (node->op == ">=") cmp = "sge";
            body_ += "  " + reg + " = icmp " + cmp + " i64 " + left.ir + ", " + right.ir + "\n";
            return {reg, Type::bool_type()};
        }
        if (node->op == "+" && left.type == Type::string_type()) {
            body_ += "  " + reg + " = call ptr @pinggen_concat(ptr " + left.ir + ", ptr " + right.ir + ")\n";
            return {reg, Type::string_type()};
        }

        std::string op;
        if (node->op == "+") op = "add";
        if (node->op == "-") op = "sub";
        if (node->op == "*") op = "mul";
        if (node->op == "/") op = "sdiv";
        if (node->op == "%") op = "srem";
        body_ += "  " + reg + " = " + op + " i64 " + left.ir + ", " + right.ir + "\n";
        return {reg, Type::int_type()};
    }
    if (const auto* node = dynamic_cast<const CallExpr*>(&expr)) {
        if (node->callee == "io::println") {
            const TypedIRValue arg = emit_expr(*node->args[0]);
            if (arg.type == Type::int_type()) {
                const std::string fmt_reg = next_register();
                const std::string call_reg = next_register();
                body_ += "  " + fmt_reg + " = getelementptr inbounds [6 x i8], ptr @.fmt.int, i64 0, i64 0\n";
                body_ += "  " + call_reg + " = call i32 (ptr, ...) @printf(ptr " + fmt_reg + ", i64 " + arg.ir + ")\n";
            } else {
                const std::string fmt_reg = next_register();
                const std::string call_reg = next_register();
                body_ += "  " + fmt_reg + " = getelementptr inbounds [4 x i8], ptr @.fmt.str, i64 0, i64 0\n";
                body_ += "  " + call_reg + " = call i32 (ptr, ...) @printf(ptr " + fmt_reg + ", ptr " + arg.ir + ")\n";
            }
            return {"0", Type::void_type()};
        }
        if (node->callee == "str::len") {
            const TypedIRValue arg = emit_expr(*node->args[0]);
            const std::string reg = next_register();
            body_ += "  " + reg + " = call i64 @strlen(ptr " + arg.ir + ")\n";
            return {reg, Type::int_type()};
        }
        if (node->callee == "fs::read_to_string") {
            const TypedIRValue arg = emit_expr(*node->args[0]);
            const std::string reg = next_register();
            body_ += "  " + reg + " = call %enum.FsResult @pinggen_fs_read_to_string(ptr " + arg.ir + ")\n";
            return {reg, Type::enum_type("FsResult")};
        }
        if (node->callee == "fs::write_string") {
            const TypedIRValue path = emit_expr(*node->args[0]);
            const TypedIRValue contents = emit_expr(*node->args[1]);
            const std::string reg = next_register();
            body_ += "  " + reg + " = call %enum.FsWriteResult @pinggen_fs_write_string(ptr " + path.ir + ", ptr " +
                     contents.ir + ")\n";
            return {reg, Type::enum_type("FsWriteResult")};
        }
        if (node->callee == "fs::exists") {
            const TypedIRValue path = emit_expr(*node->args[0]);
            const std::string reg = next_register();
            body_ += "  " + reg + " = call i1 @pinggen_fs_exists(ptr " + path.ir + ")\n";
            return {reg, Type::bool_type()};
        }

        std::vector<TypedIRValue> args;
        for (const auto& arg : node->args) {
            args.push_back(emit_expr(*arg));
        }

        const Type return_type = function_return_types_.at(node->callee);
        std::string call_args;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i != 0) {
                call_args += ", ";
            }
            call_args += llvm_type(args[i].type) + " " + args[i].ir;
        }

        if (return_type == Type::void_type()) {
            body_ += "  call void @" + node->callee + "(" + call_args + ")\n";
            return {"0", Type::void_type()};
        }

        const std::string reg = next_register();
        body_ += "  " + reg + " = call " + llvm_type(return_type) + " @" + node->callee + "(" + call_args + ")\n";
        return {reg, return_type};
    }
    if (const auto* node = dynamic_cast<const MethodCallExpr*>(&expr)) {
        std::vector<TypedIRValue> args;
        std::string lowered_name;
        bool is_mutating = false;
        if (const auto* object_var = dynamic_cast<const VariableExpr*>(node->object.get())) {
            const Type object_type = variable_types_.at(object_var->name);
            lowered_name = object_type.name + "__" + node->method;
            is_mutating = mutating_methods_.at(lowered_name);
        } else {
            const TypedIRValue object_value = emit_expr(*node->object);
            lowered_name = object_value.type.name + "__" + node->method;
            is_mutating = mutating_methods_.at(lowered_name);
            args.push_back(object_value);
        }

        if (is_mutating) {
            const AddressValue self_addr = emit_address(*node->object);
            args.clear();
            args.push_back({self_addr.address, self_addr.type});
        } else if (args.empty()) {
            args.push_back(emit_expr(*node->object));
        }
        for (const auto& arg : node->args) {
            args.push_back(emit_expr(*arg));
        }

        const Type return_type = function_return_types_.at(lowered_name);
        std::string call_args;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i != 0) {
                call_args += ", ";
            }
            if (is_mutating && i == 0) {
                call_args += "ptr " + args[i].ir;
            } else {
                call_args += llvm_type(args[i].type) + " " + args[i].ir;
            }
        }

        if (return_type == Type::void_type()) {
            body_ += "  call void @" + lowered_name + "(" + call_args + ")\n";
            return {"0", Type::void_type()};
        }

        const std::string reg = next_register();
        body_ += "  " + reg + " = call " + llvm_type(return_type) + " @" + lowered_name + "(" + call_args + ")\n";
        return {reg, return_type};
    }
    if (const auto* node = dynamic_cast<const ConExpr*>(&expr)) {
        return emit_con_expr(*node);
    }

    return {"0", Type::void_type()};
}

std::string LLVMIRGenerator::emit_enum_tag(const TypedIRValue& enum_value) {
    if (!enum_has_payload(enum_value.type.name)) {
        return enum_value.ir;
    }
    const std::string tag_reg = next_register();
    body_ += "  " + tag_reg + " = extractvalue " + llvm_type(enum_value.type) + " " + enum_value.ir + ", 0\n";
    return tag_reg;
}

bool LLVMIRGenerator::emit_block(const std::vector<std::unique_ptr<Stmt>>& body) {
    for (const auto& stmt : body) {
        if (emit_stmt(*stmt)) {
            return true;
        }
    }
    return false;
}

bool LLVMIRGenerator::emit_stmt(const Stmt& stmt) {
    if (const auto* node = dynamic_cast<const LetStmt*>(&stmt)) {
        const TypedIRValue value = emit_expr(*node->initializer);
        const std::string storage = next_register();
        body_ += "  " + storage + " = alloca " + llvm_type(value.type) + "\n";
        body_ += "  store " + llvm_type(value.type) + " " + value.ir + ", ptr " + storage + "\n";
        variables_[node->name] = storage;
        variable_types_[node->name] = value.type;
        return false;
    }
    if (const auto* node = dynamic_cast<const TupleLetStmt*>(&stmt)) {
        const TypedIRValue value = emit_expr(*node->initializer);
        for (std::size_t i = 0; i < node->names.size(); ++i) {
            const std::string element_reg = next_register();
            const std::string storage = next_register();
            const Type element_type = value.type.tuple_elements[i];
            body_ += "  " + element_reg + " = extractvalue " + llvm_type(value.type) + " " + value.ir + ", " + std::to_string(i) +
                     "\n";
            body_ += "  " + storage + " = alloca " + llvm_type(element_type) + "\n";
            body_ += "  store " + llvm_type(element_type) + " " + element_reg + ", ptr " + storage + "\n";
            variables_[node->names[i]] = storage;
            variable_types_[node->names[i]] = element_type;
        }
        return false;
    }
    if (const auto* node = dynamic_cast<const AssignStmt*>(&stmt)) {
        const TypedIRValue value = emit_expr(*node->value);
        const Type type = variable_types_.at(node->name);
        body_ += "  store " + llvm_type(type) + " " + value.ir + ", ptr " + variables_.at(node->name) + "\n";
        return false;
    }
    if (const auto* node = dynamic_cast<const FieldAssignStmt*>(&stmt)) {
        const auto object_address = emit_address(*node->object);
        const std::size_t index = struct_field_indices_.at(object_address.type.name).at(node->field);
        const Type field_type = normalize_type(structs_.at(object_address.type.name).fields[index].type);
        const TypedIRValue value = emit_expr(*node->value);
        const std::string field_ptr = next_register();
        body_ += "  " + field_ptr + " = getelementptr inbounds " + llvm_type(object_address.type) + ", ptr " +
                 object_address.address + ", i32 0, i32 " + std::to_string(index) + "\n";
        body_ += "  store " + llvm_type(field_type) + " " + value.ir + ", ptr " + field_ptr + "\n";
        return false;
    }
    if (const auto* node = dynamic_cast<const IndexAssignStmt*>(&stmt)) {
        const AddressValue object_address = emit_address(*node->object);
        const TypedIRValue index = emit_expr(*node->index);
        emit_bounds_check(index.ir, object_address.type.array_size);
        const Type element_type = *object_address.type.element_type;
        const TypedIRValue value = emit_expr(*node->value);
        const std::string element_ptr = next_register();
        body_ += "  " + element_ptr + " = getelementptr inbounds " + llvm_type(object_address.type) + ", ptr " +
                 object_address.address + ", i32 0, i64 " + index.ir + "\n";
        body_ += "  store " + llvm_type(element_type) + " " + value.ir + ", ptr " + element_ptr + "\n";
        return false;
    }
    if (const auto* node = dynamic_cast<const ExprStmt*>(&stmt)) {
        emit_expr(*node->expr);
        return false;
    }
    if (const auto* node = dynamic_cast<const IfStmt*>(&stmt)) {
        const TypedIRValue condition = emit_expr(*node->condition);
        const std::string then_label = next_label("if_then");
        const std::string else_label = next_label("if_else");
        const std::string end_label = next_label("if_end");
        body_ += "  br i1 " + condition.ir + ", label %" + then_label + ", label %" + else_label + "\n";

        body_ += then_label + ":\n";
        const bool then_returns = emit_block(node->then_body);
        if (!then_returns) {
            body_ += "  br label %" + end_label + "\n";
        }

        body_ += else_label + ":\n";
        const bool has_else = !node->else_body.empty();
        const bool else_returns = has_else && emit_block(node->else_body);
        if (!else_returns) {
            body_ += "  br label %" + end_label + "\n";
        }

        if (!(then_returns && has_else && else_returns)) {
            body_ += end_label + ":\n";
        }
        return then_returns && has_else && else_returns;
    }
    if (const auto* node = dynamic_cast<const MatchStmt*>(&stmt)) {
        const TypedIRValue subject = emit_expr(*node->subject);
        const std::string subject_tag = subject.type.kind == TypeKind::Enum ? emit_enum_tag(subject) : subject.ir;
        const std::string end_label = next_label("match_end");
        const std::string fail_label = next_label("match_unreachable");
        std::vector<std::string> arm_labels;
        std::vector<std::string> check_labels;
        std::vector<bool> arm_return_flags;
        arm_labels.reserve(node->arms.size());
        check_labels.reserve(node->arms.size());
        arm_return_flags.reserve(node->arms.size());
        for (std::size_t i = 0; i < node->arms.size(); ++i) {
            arm_labels.push_back(next_label("match_arm"));
            if (i + 1 < node->arms.size()) {
                check_labels.push_back(next_label("match_check"));
            }
        }

        for (std::size_t i = 0; i < node->arms.size(); ++i) {
            const auto& arm = node->arms[i];
            const std::string next_label_name = i + 1 < node->arms.size() ? check_labels[i] : fail_label;
            const std::string cmp_reg = next_register();
            const std::size_t ordinal = enum_variant_indices_.at(arm.enum_name).at(arm.variant);
            body_ += "  " + cmp_reg + " = icmp eq i64 " + subject_tag + ", " + std::to_string(ordinal) + "\n";
            body_ += "  br i1 " + cmp_reg + ", label %" + arm_labels[i] + ", label %" + next_label_name + "\n";
            body_ += arm_labels[i] + ":\n";

            const auto previous_var = arm.binding_name.has_value() ? variables_.find(*arm.binding_name) : variables_.end();
            const auto previous_type = arm.binding_name.has_value() ? variable_types_.find(*arm.binding_name) : variable_types_.end();
            const bool had_previous_var = arm.binding_name.has_value() && previous_var != variables_.end();
            const bool had_previous_type = arm.binding_name.has_value() && previous_type != variable_types_.end();
            const std::string previous_storage = had_previous_var ? previous_var->second : "";
            const Type previous_type_value = had_previous_type ? previous_type->second : Type::void_type();

            if (arm.binding_name.has_value()) {
                const Type payload_type = *enum_payload_type(arm.enum_name, arm.variant);
                const std::size_t payload_field_index = enum_payload_field_indices_.at(arm.enum_name).at(arm.variant);
                const std::string payload_reg = next_register();
                body_ += "  " + payload_reg + " = extractvalue " + llvm_type(subject.type) + " " + subject.ir + ", " +
                         std::to_string(payload_field_index) + "\n";
                const std::string payload_storage = next_register();
                body_ += "  " + payload_storage + " = alloca " + llvm_type(payload_type) + "\n";
                body_ += "  store " + llvm_type(payload_type) + " " + payload_reg + ", ptr " + payload_storage + "\n";
                variables_[*arm.binding_name] = payload_storage;
                variable_types_[*arm.binding_name] = payload_type;
            }

            const bool arm_returns = emit_block(arm.body);

            if (arm.binding_name.has_value()) {
                if (had_previous_var) {
                    variables_[*arm.binding_name] = previous_storage;
                } else {
                    variables_.erase(*arm.binding_name);
                }
                if (had_previous_type) {
                    variable_types_[*arm.binding_name] = previous_type_value;
                } else {
                    variable_types_.erase(*arm.binding_name);
                }
            }

            arm_return_flags.push_back(arm_returns);
            if (!arm_returns) {
                body_ += "  br label %" + end_label + "\n";
            }
            if (i + 1 < node->arms.size()) {
                body_ += check_labels[i] + ":\n";
            }
        }
        body_ += fail_label + ":\n";
        body_ += "  unreachable\n";

        bool all_return = true;
        for (bool current_arm_returns : arm_return_flags) {
            if (!current_arm_returns) {
                all_return = false;
                break;
            }
        }
        if (!all_return) {
            body_ += end_label + ":\n";
        }
        return all_return;
    }
    if (const auto* node = dynamic_cast<const WhileStmt*>(&stmt)) {
        const std::string cond_label = next_label("while_cond");
        const std::string body_label = next_label("while_body");
        const std::string end_label = next_label("while_end");

        body_ += "  br label %" + cond_label + "\n";
        body_ += cond_label + ":\n";
        const TypedIRValue condition = emit_expr(*node->condition);
        body_ += "  br i1 " + condition.ir + ", label %" + body_label + ", label %" + end_label + "\n";
        body_ += body_label + ":\n";

        continue_labels_.push_back(cond_label);
        break_labels_.push_back(end_label);
        const bool body_returns = emit_block(node->body);
        continue_labels_.pop_back();
        break_labels_.pop_back();

        if (!body_returns) {
            body_ += "  br label %" + cond_label + "\n";
        }
        body_ += end_label + ":\n";
        return false;
    }
    if (const auto* node = dynamic_cast<const ForStmt*>(&stmt)) {
        const TypedIRValue start = emit_expr(*node->start);
        const TypedIRValue end = emit_expr(*node->end);
        const std::string loop_storage = next_register();
        body_ += "  " + loop_storage + " = alloca i64\n";
        body_ += "  store i64 " + start.ir + ", ptr " + loop_storage + "\n";

        const std::string cond_label = next_label("for_cond");
        const std::string body_label = next_label("for_body");
        const std::string step_label = next_label("for_step");
        const std::string end_label = next_label("for_end");

        body_ += "  br label %" + cond_label + "\n";
        body_ += cond_label + ":\n";
        const std::string current_reg = next_register();
        const std::string cond_reg = next_register();
        body_ += "  " + current_reg + " = load i64, ptr " + loop_storage + "\n";
        body_ += "  " + cond_reg + " = icmp slt i64 " + current_reg + ", " + end.ir + "\n";
        body_ += "  br i1 " + cond_reg + ", label %" + body_label + ", label %" + end_label + "\n";
        body_ += body_label + ":\n";

        const auto previous_var = variables_.find(node->name);
        const auto previous_type = variable_types_.find(node->name);
        const bool had_previous_var = previous_var != variables_.end();
        const bool had_previous_type = previous_type != variable_types_.end();
        const std::string previous_storage = had_previous_var ? previous_var->second : "";
        const Type previous_type_value = had_previous_type ? previous_type->second : Type::void_type();

        variables_[node->name] = loop_storage;
        variable_types_[node->name] = Type::int_type();
        continue_labels_.push_back(step_label);
        break_labels_.push_back(end_label);
        const bool body_returns = emit_block(node->body);
        continue_labels_.pop_back();
        break_labels_.pop_back();
        if (had_previous_var) {
            variables_[node->name] = previous_storage;
        } else {
            variables_.erase(node->name);
        }
        if (had_previous_type) {
            variable_types_[node->name] = previous_type_value;
        } else {
            variable_types_.erase(node->name);
        }

        if (!body_returns) {
            body_ += "  br label %" + step_label + "\n";
        }
        body_ += step_label + ":\n";
        const std::string step_current = next_register();
        const std::string next_value = next_register();
        body_ += "  " + step_current + " = load i64, ptr " + loop_storage + "\n";
        body_ += "  " + next_value + " = add i64 " + step_current + ", 1\n";
        body_ += "  store i64 " + next_value + ", ptr " + loop_storage + "\n";
        body_ += "  br label %" + cond_label + "\n";
        body_ += end_label + ":\n";
        return false;
    }
    if (dynamic_cast<const BreakStmt*>(&stmt)) {
        body_ += "  br label %" + break_labels_.back() + "\n";
        return true;
    }
    if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        body_ += "  br label %" + continue_labels_.back() + "\n";
        return true;
    }
    if (const auto* node = dynamic_cast<const ReturnStmt*>(&stmt)) {
        if (!node->value) {
            if (current_function_name_ == "main") {
                body_ += "  ret i32 0\n";
            } else if (current_return_type_ == Type::void_type()) {
                body_ += "  ret void\n";
            } else if (current_return_type_ == Type::bool_type()) {
                body_ += "  ret i1 0\n";
            } else if (current_return_type_ == Type::int_type() ||
                       (current_return_type_.kind == TypeKind::Enum && !enum_has_payload(current_return_type_.name))) {
                body_ += "  ret i64 0\n";
            } else if (current_return_type_.kind == TypeKind::Enum && enum_has_payload(current_return_type_.name)) {
                body_ += "  ret " + llvm_type(current_return_type_) + " zeroinitializer\n";
            }
            return true;
        }

        const TypedIRValue value = emit_expr(*node->value);
        if (current_function_name_ == "main") {
            const std::string reg = next_register();
            body_ += "  " + reg + " = trunc i64 " + value.ir + " to i32\n";
            body_ += "  ret i32 " + reg + "\n";
        } else {
            body_ += "  ret " + llvm_type(value.type) + " " + value.ir + "\n";
        }
        return true;
    }
    return false;
}

std::string LLVMIRGenerator::llvm_type(const Type& type) const {
    switch (type.kind) {
        case TypeKind::Int: return "i64";
        case TypeKind::Bool: return "i1";
        case TypeKind::String: return "ptr";
        case TypeKind::Void: return "void";
        case TypeKind::Enum: return enum_has_payload(type.name) ? "%enum." + type.name : "i64";
        case TypeKind::Struct: return "%struct." + type.name;
        case TypeKind::Array: return "[" + std::to_string(type.array_size) + " x " + llvm_type(*type.element_type) + "]";
        case TypeKind::Tuple: {
            std::string out = "{ ";
            for (std::size_t i = 0; i < type.tuple_elements.size(); ++i) {
                if (i != 0) {
                    out += ", ";
                }
                out += llvm_type(type.tuple_elements[i]);
            }
            out += " }";
            return out;
        }
    }
    return "void";
}

std::string LLVMIRGenerator::next_label(const std::string& prefix) {
    return prefix + "." + std::to_string(++label_counter_);
}

void LLVMIRGenerator::reset_function_state() {
    body_.clear();
    variables_.clear();
    variable_types_.clear();
    register_counter_ = 0;
}

std::string LLVMIRGenerator::next_register() {
    return "%" + std::to_string(++register_counter_);
}

std::string LLVMIRGenerator::next_string_name() {
    return ".str." + std::to_string(++string_counter_);
}

std::string LLVMIRGenerator::escape_bytes(const std::string& value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        switch (c) {
            case '\n': out << "\\0A"; break;
            case '\t': out << "\\09"; break;
            case '\"': out << "\\22"; break;
            case '\\': out << "\\5C"; break;
            default:
                if (c >= 32 && c <= 126) {
                    out << static_cast<char>(c);
                } else {
                    const char* digits = "0123456789ABCDEF";
                    out << '\\' << digits[(c >> 4) & 0xF] << digits[c & 0xF];
                }
        }
    }
    return out.str();
}

}  // namespace pinggen
