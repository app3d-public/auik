#pragma once

#include <acul/functional/unique_function.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/hash/hashset.hpp>
#include <acul/hash/utils.hpp>
#include <acul/memory/alloc.hpp>
#include <acul/scalars.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include <algorithm>
#include <amal/geometric.hpp>
#include <auik/symbol_export.h>


#define AUIK_MODEL_RECORD_ID_INVALID 0u
#define AUIK_MODEL_FIELD_ID_INVALID  0u

namespace auik
{
    namespace mqa
    {
        class ModelQueryBuilder;
    }

    class Widget;
    struct Model;
    struct ModelBinding;
    struct ModelField;

    using ModelID = u64;
    using ModelRecordID = u64;
    using ModelFieldID = u32;
    using ModelPipelineID = u64;

    enum class ModelRecordsOp : u8
    {
        create,
        destroy,
        move,
        order,
        reset
    };

    struct ModelRecordsEvent
    {
        ModelRecordsOp op = ModelRecordsOp::reset;
        ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID;
        u32 from_index = 0u;
        u32 to_index = 0u;
        u32 count = 1u;
    };

    struct ModelBatchData
    {
        const void *data = nullptr;
        const void *const *items = nullptr;
        u32 stride = 0u;
        u32 count = 0u;

        bool contiguous() const { return data != nullptr && stride != 0u; }
        bool indexed() const { return items != nullptr; }
        explicit operator bool() const { return count != 0u && (contiguous() || indexed()); }
    };

    struct ModelBinding;
    struct ModelFieldModelBinding
    {
        ModelBinding *binding = nullptr;
        ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID;
    };
    AUIK_EXPORT void dispatch_model_field(ModelField &field);

    struct ModelField
    {
        using model_binding_container = acul::vector<ModelFieldModelBinding>;
        using value_type = ModelFieldModelBinding;
        using iterator = model_binding_container::iterator;
        using const_iterator = model_binding_container::const_iterator;
        using model_binding_iterator = model_binding_container::iterator;
        using const_model_binding_iterator = model_binding_container::const_iterator;

        ModelFieldID id = AUIK_MODEL_FIELD_ID_INVALID;
        model_binding_container model_bindings;

        virtual ~ModelField() = default;
        virtual void *data() { return nullptr; }
        virtual const void *data() const { return nullptr; }
        virtual u32 size() const { return 0u; }
        virtual void release() { acul::release(this); }

        template <class T>
        T *as()
        {
            return static_cast<T *>(data());
        }

        template <class T>
        const T *as() const
        {
            return static_cast<const T *>(data());
        }

        iterator begin() { return model_bindings.begin(); }
        iterator end() { return model_bindings.end(); }
        const_iterator begin() const { return model_bindings.begin(); }
        const_iterator end() const { return model_bindings.end(); }
        const_iterator cbegin() const { return model_bindings.cbegin(); }
        const_iterator cend() const { return model_bindings.cend(); }
        model_binding_iterator model_bindings_begin() { return model_bindings.begin(); }
        model_binding_iterator model_bindings_end() { return model_bindings.end(); }
        const_model_binding_iterator model_bindings_begin() const { return model_bindings.begin(); }
        const_model_binding_iterator model_bindings_end() const { return model_bindings.end(); }
        const_model_binding_iterator model_bindings_cbegin() const { return model_bindings.cbegin(); }
        const_model_binding_iterator model_bindings_cend() const { return model_bindings.cend(); }
    };

    struct ModelRefField final : ModelField
    {
        void *ptr = nullptr;
        u32 byte_size = 0u;

        ModelRefField() = default;
        ModelRefField(ModelFieldID field_id, void *field_data, u32 field_size) : ptr(field_data), byte_size(field_size)
        {
            id = field_id;
        }

        void *data() override { return ptr; }
        const void *data() const override { return ptr; }
        u32 size() const override { return byte_size; }
        void release() override { acul::release(this); }
    };

    template <class T>
    struct ModelValueField final : ModelField
    {
        T value{};

        ModelValueField() = default;
        explicit ModelValueField(ModelFieldID field_id) { id = field_id; }
        ModelValueField(ModelFieldID field_id, T field_value) : value(std::move(field_value)) { id = field_id; }

        template <class U>
        void set_value(U &&next)
        {
            value = T(std::forward<U>(next));
            dispatch_model_field(*this);
        }

        void *data() override { return &value; }
        const void *data() const override { return &value; }
        u32 size() const override { return sizeof(T); }
        void release() override { acul::release(this); }
    };

    template <class T, class... Args>
    inline ModelValueField<T> *make_model_field(ModelFieldID field_id, Args &&...args)
    {
        return acul::alloc<ModelValueField<T>>(field_id, T(std::forward<Args>(args)...));
    }

    struct ModelRecord
    {
        using field_container = acul::vector<ModelField *>;
        using value_type = ModelField *;
        using iterator = field_container::iterator;
        using const_iterator = field_container::const_iterator;

        ModelRecordID id = AUIK_MODEL_RECORD_ID_INVALID;
        void *data = nullptr;
        field_container fields;

        template <class T>
        T *as()
        {
            return static_cast<T *>(data);
        }

        template <class T>
        const T *as() const
        {
            return static_cast<const T *>(data);
        }

        iterator begin() { return fields.begin(); }
        iterator end() { return fields.end(); }
        const_iterator begin() const { return fields.begin(); }
        const_iterator end() const { return fields.end(); }
        const_iterator cbegin() const { return fields.cbegin(); }
        const_iterator cend() const { return fields.cend(); }
    };

    using PFN_model_record_id = ModelRecordID (*)(ModelRecord &record);
    using PFN_model_field_data_batch = ModelBatchData (*)(const ModelRecordID *record_ids, u32 count,
                                                          ModelFieldID field_id, void *data);
    using PFN_present_model_record = void (*)(ModelBinding *binding, ModelRecord &record, u32 record_index,
                                              Widget **widgets, u32 widget_count, void *data);

    struct ModelRecordPresenter
    {
        void *data = nullptr;
        acul::vector<ModelFieldID> field_ids;
        PFN_present_model_record present_record = nullptr;
    };

    AUIK_EXPORT ModelRecordID make_generated_model_record_id(ModelRecord &record);

    AUIK_EXPORT ModelField *find_model_field(ModelRecord &record, ModelFieldID field_id);
    AUIK_EXPORT const ModelField *find_model_field(const ModelRecord &record, ModelFieldID field_id);

    struct Model
    {
        using record_container = acul::vector<ModelRecord>;
        using record_index_container = acul::hashmap<ModelRecordID, u32>;
        using binding_container = acul::vector<ModelBinding *>;
        using value_type = ModelRecord;
        using iterator = record_container::iterator;
        using const_iterator = record_container::const_iterator;
        using binding_iterator = binding_container::iterator;
        using const_binding_iterator = binding_container::const_iterator;

        void *data = nullptr;
        PFN_model_record_id make_record_id_cb = make_generated_model_record_id;
        PFN_model_field_data_batch field_data_batch_callback = nullptr;
        record_container records;
        record_index_container record_indices;
        binding_container bindings;

        template <class T>
        T *as()
        {
            return static_cast<T *>(data);
        }

        template <class T>
        const T *as() const
        {
            return static_cast<const T *>(data);
        }

        inline u32 record_count() const { return static_cast<u32>(records.size()); }

        mqa::ModelQueryBuilder query() const;

        inline ModelRecordID record_id_at(u32 index) const
        {
            return index < records.size() ? records[index].id : AUIK_MODEL_RECORD_ID_INVALID;
        }

        inline u32 record_index(ModelRecordID record_id) const
        {
            const auto it = record_indices.find(record_id);
            if (it != record_indices.end()) return it->second;
            return 0xFFFFFFFFu;
        }

        inline u32 field_count() const { return records.empty() ? 0u : static_cast<u32>(records[0].fields.size()); }

        inline ModelFieldID field_id_at(u32 index) const
        {
            if (records.empty() || index >= records[0].fields.size()) return AUIK_MODEL_FIELD_ID_INVALID;
            return records[0].fields[index] ? records[0].fields[index]->id : AUIK_MODEL_FIELD_ID_INVALID;
        }
        inline u32 field_index(ModelFieldID field_id) const
        {
            const u32 count = field_count();
            for (u32 index = 0; index < count; ++index)
                if (field_id_at(index) == field_id) return index;
            return 0xFFFFFFFFu;
        }

        inline ModelRecord *find_record(ModelRecordID record_id)
        {
            const u32 index = record_index(record_id);
            return index < records.size() ? &records[index] : nullptr;
        }

        inline const ModelRecord *find_record(ModelRecordID record_id) const
        {
            const u32 index = record_index(record_id);
            return index < records.size() ? &records[index] : nullptr;
        }

        inline void *field_data(ModelRecordID record_id, ModelFieldID field_id)
        {
            auto *record = find_record(record_id);
            auto *field = record ? find_model_field(*record, field_id) : nullptr;
            return field ? field->data() : nullptr;
        }

        inline const void *field_data(ModelRecordID record_id, ModelFieldID field_id) const
        {
            const auto *record = find_record(record_id);
            const auto *field = record ? find_model_field(*record, field_id) : nullptr;
            return field ? field->data() : nullptr;
        }

        AUIK_EXPORT bool write_field(ModelRecordID record_id, ModelFieldID field_id, const void *src);

        inline ModelBatchData field_data_batch(const ModelRecordID *record_ids, u32 count, ModelFieldID field_id) const
        {
            if (field_data_batch_callback) return field_data_batch_callback(record_ids, count, field_id, data);
            return {};
        }

        inline void dispatch_records(const ModelRecordsEvent &event);

        inline ModelRecordID make_record_id(ModelRecord &record)
        {
            return make_record_id_cb ? make_record_id_cb(record) : record.id;
        }

        inline ModelRecord &add_record(ModelRecord record)
        {
            record.id = make_record_id(record);
            records.push_back(std::move(record));
            if (records.back().id != AUIK_MODEL_RECORD_ID_INVALID)
            {
                record_indices.emplace(records.back().id, static_cast<u32>(records.size() - 1u));
            }
            return records.back();
        }

        inline ModelRecord &add_record(ModelRecordID record_id, void *data)
        {
            ModelRecord record{};
            record.id = record_id;
            record.data = data;
            return add_record(std::move(record));
        }

        iterator begin() { return records.begin(); }
        iterator end() { return records.end(); }
        const_iterator begin() const { return records.begin(); }
        const_iterator end() const { return records.end(); }
        const_iterator cbegin() const { return records.cbegin(); }
        const_iterator cend() const { return records.cend(); }

        binding_iterator bindings_begin() { return bindings.begin(); }
        binding_iterator bindings_end() { return bindings.end(); }
        const_binding_iterator bindings_begin() const { return bindings.begin(); }
        const_binding_iterator bindings_end() const { return bindings.end(); }
        const_binding_iterator bindings_cbegin() const { return bindings.cbegin(); }
        const_binding_iterator bindings_cend() const { return bindings.cend(); }
    };

    inline ModelField *add_model_field(ModelRecord &record, ModelField *field)
    {
        if (!field) return nullptr;
        record.fields.push_back(field);
        return field;
    }

    AUIK_EXPORT void release_model_record_fields(ModelRecord &record);

    template <class T = acul::string>
    inline ModelValueField<T> *get_value_model_field(Model *model,
                                                     ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID,
                                                     ModelFieldID field_id = 1u)
    {
        if (!model || model->records.empty()) return nullptr;
        auto *record = record_id == AUIK_MODEL_RECORD_ID_INVALID ? &model->records[0] : model->find_record(record_id);
        auto *field = record ? find_model_field(*record, field_id) : nullptr;
        return field ? static_cast<ModelValueField<T> *>(field) : nullptr;
    }

    template <class T = acul::string>
    inline const ModelValueField<T> *get_value_model_field(const Model *model,
                                                           ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID,
                                                           ModelFieldID field_id = 1u)
    {
        if (!model || model->records.empty()) return nullptr;
        const auto *record =
            record_id == AUIK_MODEL_RECORD_ID_INVALID ? &model->records[0] : model->find_record(record_id);
        const auto *field = record ? find_model_field(*record, field_id) : nullptr;
        return field ? static_cast<const ModelValueField<T> *>(field) : nullptr;
    }

    namespace mqa
    {
        template <class T>
        struct ModelValueQueryAccess
        {
            static const T *get(const ModelField *field)
            {
                return field ? &static_cast<const ModelValueField<T> *>(field)->value : nullptr;
            }
        };

        template <class T>
        struct ModelRefQueryAccess
        {
            static const T *get(const ModelField *field)
            {
                const auto *ref = static_cast<const ModelRefField *>(field);
                return ref && ref->ptr && ref->byte_size == sizeof(T) ? static_cast<const T *>(ref->ptr) : nullptr;
            }
        };

        template <class Query>
        inline u32 query_begin_index(const Query &query)
        {
            if constexpr (requires { query.begin_index(); }) return query.begin_index();
            return 0u;
        }

        template <class Query>
        inline u32 query_end_index(const Query &query, u32 record_count)
        {
            if constexpr (requires { query.end_index(); })
            {
                const u32 end = query.end_index();
                return end < record_count ? end : record_count;
            }
            return record_count;
        }

        template <class T, class Predicate, class Access = ModelValueQueryAccess<T>>
        struct ModelFieldQuery
        {
            ModelFieldID field_id = AUIK_MODEL_FIELD_ID_INVALID;
            Predicate predicate;
            u32 field_index = 0xFFFFFFFFu;

            void prepare(const Model &model) { field_index = model.field_index(field_id); }

            bool matches(const ModelRecord &record) const
            {
                if (field_index >= record.fields.size()) return false;
                const auto *field = record.fields[field_index];
                if (!field || field->id != field_id) return false;
                const auto *value = Access::get(field);
                return value && predicate(*value);
            }
        };

        template <class Predicate>
        struct ModelRecordIDQuery
        {
            Predicate predicate;

            void prepare(const Model &) {}
            bool matches(const ModelRecord &record) const { return predicate(record.id); }
        };

        template <class Left, class Right>
        struct ModelQueryAll
        {
            Left left;
            Right right;

            void prepare(const Model &model)
            {
                left.prepare(model);
                right.prepare(model);
            }

            bool matches(const ModelRecord &record) const { return left.matches(record) && right.matches(record); }

            u32 begin_index() const
            {
                const u32 left_begin = query_begin_index(left);
                const u32 right_begin = query_begin_index(right);
                return left_begin > right_begin ? left_begin : right_begin;
            }

            u32 end_index() const
            {
                const u32 left_end = query_end_index(left, 0xFFFFFFFFu);
                const u32 right_end = query_end_index(right, 0xFFFFFFFFu);
                return left_end < right_end ? left_end : right_end;
            }
        };

        template <class Left, class Right>
        struct ModelQueryAny
        {
            Left left;
            Right right;

            void prepare(const Model &model)
            {
                left.prepare(model);
                right.prepare(model);
            }

            bool matches(const ModelRecord &record) const { return left.matches(record) || right.matches(record); }

            u32 begin_index() const
            {
                const u32 left_begin = query_begin_index(left);
                const u32 right_begin = query_begin_index(right);
                return left_begin < right_begin ? left_begin : right_begin;
            }

            u32 end_index() const
            {
                const u32 left_end = query_end_index(left, 0xFFFFFFFFu);
                const u32 right_end = query_end_index(right, 0xFFFFFFFFu);
                return left_end > right_end ? left_end : right_end;
            }
        };

        template <class T>
        struct ModelQueryEqual
        {
            T value;
            bool operator()(const T &candidate) const { return candidate == value; }
        };

        template <class T>
        struct ModelQueryNotEqual
        {
            T value;
            bool operator()(const T &candidate) const { return candidate != value; }
        };

        template <class T>
        struct ModelQueryIn
        {
            acul::hashset<T> values;
            bool operator()(const T &candidate) const { return values.count(candidate) != 0u; }
        };

        struct ModelRecordIDInQuery
        {
            acul::hashset<ModelRecordID> values;
            u32 first_record = 0u;
            u32 last_record = 0u;

            void prepare(const Model &model)
            {
                first_record = model.record_count();
                last_record = 0u;
                for (ModelRecordID record_id : values)
                {
                    const u32 index = model.record_index(record_id);
                    if (index >= model.record_count()) continue;
                    if (index < first_record) first_record = index;
                    if (index + 1u > last_record) last_record = index + 1u;
                }
                if (first_record == model.record_count()) first_record = last_record = 0u;
            }

            bool matches(const ModelRecord &record) const { return values.count(record.id) != 0u; }
            u32 begin_index() const { return first_record; }
            u32 end_index() const { return last_record; }
        };

        template <class T, class Predicate, class Access = ModelValueQueryAccess<T>>
        inline auto where(ModelFieldID field_id, Predicate predicate)
        {
            return ModelFieldQuery<T, Predicate, Access>{field_id, std::move(predicate)};
        }

        template <class T, class Predicate>
        inline auto where_ref(ModelFieldID field_id, Predicate predicate)
        {
            return where<T, Predicate, ModelRefQueryAccess<T>>(field_id, std::move(predicate));
        }

        template <class Predicate>
        inline auto where_record_id(Predicate predicate)
        {
            return ModelRecordIDQuery<Predicate>{std::move(predicate)};
        }

        template <class T>
        inline auto equal(ModelFieldID field_id, T value)
        {
            return where<T>(field_id, ModelQueryEqual<T>{std::move(value)});
        }

        template <class T>
        inline auto not_equal(ModelFieldID field_id, T value)
        {
            return where<T>(field_id, ModelQueryNotEqual<T>{std::move(value)});
        }

        inline auto record_id_equal(ModelRecordID record_id)
        {
            return where_record_id(ModelQueryEqual<ModelRecordID>{record_id});
        }

        inline auto record_id_not_equal(ModelRecordID record_id)
        {
            return where_record_id(ModelQueryNotEqual<ModelRecordID>{record_id});
        }

        template <class Range>
        inline auto record_id_in(const Range &record_ids)
        {
            ModelRecordIDInQuery query;
            for (ModelRecordID record_id : record_ids) query.values.emplace(record_id);
            return query;
        }

        inline auto record_id_in(const ModelRecordID *record_ids, size_t count)
        {
            ModelRecordIDInQuery query;
            query.values.reserve(count);
            if (record_ids)
                for (size_t index = 0u; index < count; ++index) query.values.emplace(record_ids[index]);
            return query;
        }

        template <class T, class Range>
        inline auto field_in(ModelFieldID field_id, const Range &values)
        {
            ModelQueryIn<T> predicate;
            for (const auto &value : values) predicate.values.emplace(value);
            return where<T>(field_id, std::move(predicate));
        }

        template <class Query>
        inline Query all(Query query)
        {
            return query;
        }

        template <class First, class Second, class... Rest>
        inline auto all(First first, Second second, Rest... rest)
        {
            auto right = all(std::move(second), std::move(rest)...);
            return ModelQueryAll<First, decltype(right)>{std::move(first), std::move(right)};
        }

        template <class Query>
        inline Query any(Query query)
        {
            return query;
        }

        template <class First, class Second, class... Rest>
        inline auto any(First first, Second second, Rest... rest)
        {
            auto right = any(std::move(second), std::move(rest)...);
            return ModelQueryAny<First, decltype(right)>{std::move(first), std::move(right)};
        }

        template <class Query>
        inline void select_model_records(const Model &model, Query query, acul::vector<ModelRecordID> &out)
        {
            out.clear();
            query.prepare(model);
            const u32 begin = query_begin_index(query);
            const u32 end = query_end_index(query, model.record_count());
            if (begin >= end) return;
            out.reserve(end - begin);
            for (u32 index = begin; index < end; ++index)
                if (query.matches(model.records[index])) out.push_back(model.records[index].id);
        }

        template <class Query>
        inline acul::vector<ModelRecordID> select_model_records(const Model &model, Query query)
        {
            acul::vector<ModelRecordID> result;
            select_model_records(model, std::move(query), result);
            return result;
        }

        template <class T, class ParentSelector, class Access = ModelValueQueryAccess<T>>
        struct ModelRelation
        {
            ModelFieldID field_id = AUIK_MODEL_FIELD_ID_INVALID;
            ParentSelector parent_selector;
            u32 field_index = 0xFFFFFFFFu;

            void prepare(const Model &model) { field_index = model.field_index(field_id); }

            bool parent_id(const ModelRecord &record, ModelRecordID &out) const
            {
                if (field_index >= record.fields.size()) return false;
                const auto *field = record.fields[field_index];
                if (!field || field->id != field_id) return false;
                const auto *value = Access::get(field);
                if (!value) return false;
                out = static_cast<ModelRecordID>(parent_selector(*value));
                return true;
            }
        };

        template <class T, class ParentSelector, class Access = ModelValueQueryAccess<T>>
        inline auto relation(ModelFieldID field_id, ParentSelector parent_selector)
        {
            return ModelRelation<T, ParentSelector, Access>{field_id, std::move(parent_selector)};
        }

        template <class Seed, class Relation>
        struct ModelRecursiveQuery
        {
            Seed seed;
            Relation relation;
            bool include_seed = true;
            acul::hashset<ModelRecordID> records;

            void prepare(const Model &model)
            {
                records.clear();
                seed.prepare(model);
                relation.prepare(model);

                acul::hashmap<ModelRecordID, acul::vector<ModelRecordID>> children;
                children.reserve(model.records.size());
                acul::vector<ModelRecordID> stack;
                for (const auto &record : model.records)
                {
                    ModelRecordID parent = AUIK_MODEL_RECORD_ID_INVALID;
                    if (relation.parent_id(record, parent)) children[parent].push_back(record.id);
                    if (seed.matches(record) && records.emplace(record.id).second) stack.push_back(record.id);
                }

                acul::hashset<ModelRecordID> seed_records;
                if (!include_seed) seed_records = records;
                while (!stack.empty())
                {
                    const ModelRecordID parent = stack.back();
                    stack.pop_back();
                    const auto children_it = children.find(parent);
                    if (children_it == children.end()) continue;
                    for (ModelRecordID child : children_it->second)
                        if (records.emplace(child).second) stack.push_back(child);
                }
                if (!include_seed)
                    for (ModelRecordID record_id : seed_records) records.erase(record_id);
            }

            bool matches(const ModelRecord &record) const { return records.count(record.id) != 0u; }
        };

        template <class Seed, class Relation>
        inline auto recursive(Seed seed, Relation relation, bool include_seed = true)
        {
            return ModelRecursiveQuery<Seed, Relation>{std::move(seed), std::move(relation), include_seed};
        }

        template <class Relation>
        struct ModelOrderedRecursiveQuery
        {
            acul::vector<ModelRecordID> roots;
            Relation relation;
            bool include_roots = true;
            acul::hashset<ModelRecordID> records;
            u32 first_record = 0u;
            u32 last_record = 0u;

            void prepare(const Model &model)
            {
                records.clear();
                first_record = model.record_count();
                last_record = 0u;
                relation.prepare(model);
                acul::hashset<ModelRecordID> visited;
                visited.reserve(roots.size());

                for (ModelRecordID root : roots)
                {
                    const u32 root_index = model.record_index(root);
                    if (root_index >= model.records.size() || !visited.emplace(root).second) continue;
                    if (root_index < first_record) first_record = root_index;
                    if (root_index + 1u > last_record) last_record = root_index + 1u;
                    if (include_roots) records.emplace(root);

                    for (u32 index = root_index + 1u; index < model.records.size(); ++index)
                    {
                        const auto &record = model.records[index];
                        ModelRecordID parent = AUIK_MODEL_RECORD_ID_INVALID;
                        if (!relation.parent_id(record, parent) || visited.count(parent) == 0u) break;
                        visited.emplace(record.id);
                        records.emplace(record.id);
                        if (index + 1u > last_record) last_record = index + 1u;
                    }
                }

                if (first_record == model.record_count()) first_record = last_record = 0u;
            }

            bool matches(const ModelRecord &record) const { return records.count(record.id) != 0u; }
            u32 begin_index() const { return first_record; }
            u32 end_index() const { return last_record; }
        };

        template <class Relation>
        inline auto recursive_ordered(ModelRecordID root, Relation relation, bool include_root = true)
        {
            acul::vector<ModelRecordID> roots{root};
            return ModelOrderedRecursiveQuery<Relation>{std::move(roots), std::move(relation), include_root};
        }

        template <class Relation>
        inline auto recursive_ordered(acul::vector<ModelRecordID> roots, Relation relation, bool include_roots = true)
        {
            return ModelOrderedRecursiveQuery<Relation>{std::move(roots), std::move(relation), include_roots};
        }

        template <class Relation>
        inline void select_model_records(const Model &model, ModelOrderedRecursiveQuery<Relation> query,
                                         acul::vector<ModelRecordID> &out)
        {
            out.clear();
            query.relation.prepare(model);

            acul::vector<u32> root_indices;
            root_indices.reserve(query.roots.size());
            for (ModelRecordID root : query.roots)
            {
                const u32 index = model.record_index(root);
                if (index < model.record_count()) root_indices.push_back(index);
            }
            std::sort(root_indices.begin(), root_indices.end());

            u32 covered_end = 0u;
            for (u32 root_index : root_indices)
            {
                if (root_index < covered_end) continue;

                u32 subtree_end = root_index + 1u;
                while (subtree_end < model.record_count())
                {
                    ModelRecordID parent_id = AUIK_MODEL_RECORD_ID_INVALID;
                    if (!query.relation.parent_id(model.records[subtree_end], parent_id)) break;
                    const u32 parent_index = model.record_index(parent_id);
                    if (parent_index < root_index || parent_index >= subtree_end) break;
                    ++subtree_end;
                }

                const u32 first = query.include_roots ? root_index : root_index + 1u;
                out.reserve(out.size() + subtree_end - first);
                for (u32 index = first; index < subtree_end; ++index) out.push_back(model.records[index].id);
                covered_end = subtree_end;
            }
        }

        template <class Relation>
        inline acul::vector<ModelRecordID> select_model_records(const Model &model,
                                                                ModelOrderedRecursiveQuery<Relation> query)
        {
            acul::vector<ModelRecordID> result;
            select_model_records(model, std::move(query), result);
            return result;
        }

        template <class Query>
        struct ModelQueryUntil
        {
            Query query;
            bool inclusive = false;

            void prepare(const Model &model) { query.prepare(model); }
            bool matches(const ModelRecord &record) const { return query.matches(record); }
        };

        template <class Query>
        inline auto until(Query query)
        {
            return ModelQueryUntil<Query>{std::move(query), false};
        }

        template <class Query>
        inline auto through(Query query)
        {
            return ModelQueryUntil<Query>{std::move(query), true};
        }

        template <class Query, class Stop>
        inline void select_model_records(const Model &model, Query query, ModelQueryUntil<Stop> stop,
                                         acul::vector<ModelRecordID> &out)
        {
            out.clear();
            query.prepare(model);
            stop.prepare(model);
            const u32 begin = query_begin_index(query);
            const u32 end = query_end_index(query, model.record_count());
            if (begin >= end) return;
            out.reserve(end - begin);
            for (u32 index = begin; index < end; ++index)
            {
                const auto &record = model.records[index];
                if (stop.matches(record))
                {
                    if (stop.inclusive && query.matches(record)) out.push_back(record.id);
                    break;
                }
                if (query.matches(record)) out.push_back(record.id);
            }
        }

        template <class Query, class Stop>
        inline acul::vector<ModelRecordID> select_model_records(const Model &model, Query query,
                                                                ModelQueryUntil<Stop> stop)
        {
            acul::vector<ModelRecordID> result;
            select_model_records(model, std::move(query), std::move(stop), result);
            return result;
        }

        template <class Query>
        inline ModelRecordID select_first_model_record(const Model &model, Query query)
        {
            query.prepare(model);
            const u32 begin = query_begin_index(query);
            const u32 end = query_end_index(query, model.record_count());
            for (u32 index = begin; index < end; ++index)
                if (query.matches(model.records[index])) return model.records[index].id;
            return AUIK_MODEL_RECORD_ID_INVALID;
        }

        namespace field
        {
            struct RecordID
            {
            };

            template <class T>
            struct Value
            {
                ModelFieldID field_id = AUIK_MODEL_FIELD_ID_INVALID;
            };

            inline constexpr RecordID id{};

            template <class T>
            inline Value<T> value(ModelFieldID field_id)
            {
                return {field_id};
            }
        } // namespace field

        class ModelRecordIDWhereBuilder
        {
        public:
            explicit ModelRecordIDWhereBuilder(const Model &model) : _model(model) {}

            template <class Range>
            acul::vector<ModelRecordID> in(const Range &record_ids) const
            {
                return select_model_records(_model, record_id_in(record_ids));
            }

            acul::vector<ModelRecordID> in(const ModelRecordID *record_ids, size_t count) const
            {
                return select_model_records(_model, record_id_in(record_ids, count));
            }

        private:
            const Model &_model;
        };

        template <class T>
        class ModelFieldWhereBuilder
        {
        public:
            ModelFieldWhereBuilder(const Model &model, ModelFieldID field_id) : _model(model), _field_id(field_id) {}

            template <class Range>
            acul::vector<ModelRecordID> in(const Range &values) const
            {
                return select_model_records(_model, field_in<T>(_field_id, values));
            }

        private:
            const Model &_model;
            ModelFieldID _field_id = AUIK_MODEL_FIELD_ID_INVALID;
        };

        class ModelQueryBuilder
        {
        public:
            explicit ModelQueryBuilder(const Model &model) : _model(model) {}

            ModelQueryBuilder all() const { return *this; }
            ModelRecordIDWhereBuilder where(field::RecordID) const { return ModelRecordIDWhereBuilder(_model); }

            template <class T>
            ModelFieldWhereBuilder<T> where(field::Value<T> value) const
            {
                return ModelFieldWhereBuilder<T>(_model, value.field_id);
            }

        private:
            const Model &_model;
        };
    } // namespace mqa

    inline mqa::ModelQueryBuilder Model::query() const { return mqa::ModelQueryBuilder(*this); }

    using PFN_model_pipeline_process = bool (*)(void *data, const void *src, void *dst);
    struct ModelPipelineNode
    {
        ModelPipelineID id = 0u;
        PFN_model_pipeline_process process = nullptr;
        PFN_model_pipeline_process process_reverse = nullptr;
        u32 src_size = 0u;
        u32 dst_size = 0u;
        void *data = nullptr;
        ModelPipelineNode *next = nullptr;
    };

    struct ModelPipelineCacheKey
    {
        ModelID model_id = 0u;
        ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID;
        ModelFieldID field_id = AUIK_MODEL_FIELD_ID_INVALID;
        ModelPipelineID pipeline_id = 0u;
    };

    inline bool operator==(const ModelPipelineCacheKey &a, const ModelPipelineCacheKey &b)
    {
        return a.model_id == b.model_id && a.record_id == b.record_id && a.field_id == b.field_id &&
               a.pipeline_id == b.pipeline_id;
    }

    using PFN_destroy_model_pipeline_cache_value = void (*)(void *);

    struct ModelPipelineCacheEntry
    {
        ModelPipelineCacheKey key;
        void *data = nullptr;
        u32 size = 0u;
        PFN_destroy_model_pipeline_cache_value destroy = nullptr;
    };

    using PFN_destroy_model = void (*)(Model *);
    using PFN_destroy_model_pipeline = void (*)(ModelPipelineNode *);

    struct ModelDB
    {
        struct ModelEntry
        {
            ModelID id = 0u;
            Model model;
            PFN_destroy_model destroy = nullptr;
        };

        struct PipelineEntry
        {
            u64 id = 0u;
            ModelPipelineNode *pipeline = nullptr;
            PFN_destroy_model_pipeline destroy = nullptr;
        };

        using model_container = acul::hashmap<ModelID, ModelEntry>;
        using pipeline_container = acul::hashmap<u64, PipelineEntry>;
        using binding_container = acul::vector<ModelBinding *>;
        using model_iterator = model_container::iterator;
        using const_model_iterator = model_container::const_iterator;
        using pipeline_iterator = pipeline_container::iterator;
        using const_pipeline_iterator = pipeline_container::const_iterator;

        model_container models;
        pipeline_container pipelines;
        acul::vector<u8> _pipeline_allocation_pool;
        acul::vector<ModelPipelineCacheEntry> _pipeline_cache_entries;
        u32 _pipeline_allocation_input_stride = 0u;
        u32 _pipeline_allocation_count = 0u;
        u32 _pipeline_cache_scope_depth = 0u;
        bool synced = false;

        ~ModelDB() { clear(); }

        AUIK_EXPORT void clear();
        // Takes ownership of binding and activates it for this database.
        AUIK_EXPORT bool register_binding(ModelBinding *binding);
        AUIK_EXPORT bool unregister_binding(ModelBinding *binding);

    private:
        binding_container _bindings;

    public:
        model_iterator models_begin() { return models.begin(); }
        model_iterator models_end() { return models.end(); }
        const_model_iterator models_begin() const { return models.begin(); }
        const_model_iterator models_end() const { return models.end(); }
        const_model_iterator models_cbegin() const { return models.cbegin(); }
        const_model_iterator models_cend() const { return models.cend(); }

        pipeline_iterator pipelines_begin() { return pipelines.begin(); }
        pipeline_iterator pipelines_end() { return pipelines.end(); }
        const_pipeline_iterator pipelines_begin() const { return pipelines.begin(); }
        const_pipeline_iterator pipelines_end() const { return pipelines.end(); }
        const_pipeline_iterator pipelines_cbegin() const { return pipelines.cbegin(); }
        const_pipeline_iterator pipelines_cend() const { return pipelines.cend(); }
    };

    struct ModelFieldAccess
    {
        ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID;
        ModelFieldID field_id = AUIK_MODEL_FIELD_ID_INVALID;
        ModelPipelineNode *pipeline = nullptr;
    };

    using PFN_model_binding_dispatch = void (*)(ModelBinding *, const ModelRecordsEvent &);
    using PFN_model_binding_field_dispatch = void (*)(ModelBinding *, ModelRecordID, ModelFieldID);
    using PFN_model_binding_access_field = bool (*)(ModelBinding *, ModelRecordID, ModelFieldID, ModelFieldAccess *);

    struct ModelBinding
    {
        ModelDB *db = nullptr;
        ModelID model_id = 0u;
        acul::vector<ModelRecordID> records;
        void *access_data = nullptr;
        ModelFieldAccess default_access;
        ModelRecordPresenter presenter;
        acul::unique_function<void(const ModelRecordsEvent &)> on_records = nullptr;
        acul::unique_function<void(ModelRecordID, ModelFieldID)> on_field_change = nullptr;
        PFN_model_binding_dispatch on_dispatch = nullptr;
        PFN_model_binding_field_dispatch on_field_dispatch = nullptr;
        PFN_model_binding_access_field access_field = nullptr;
    };

    inline Model *find_model(const ModelDB *db, ModelID id)
    {
        if (!db) return nullptr;
        const auto it = db->models.find(id);
        return it != db->models.end() ? const_cast<Model *>(&it->second.model) : nullptr;
    }

    inline const Model *find_const_model(const ModelDB *db, ModelID id)
    {
        if (!db) return nullptr;
        const auto it = db->models.find(id);
        return it != db->models.end() ? &it->second.model : nullptr;
    }

    inline ModelPipelineNode *find_model_pipeline(const ModelDB *db, u64 id)
    {
        if (!db) return nullptr;
        const auto it = db->pipelines.find(id);
        return it != db->pipelines.end() ? it->second.pipeline : nullptr;
    }

    AUIK_EXPORT bool register_model(ModelDB *db, ModelID id, Model model, PFN_destroy_model destroy = nullptr);
    AUIK_EXPORT bool unregister_model(ModelDB *db, ModelID id);
    AUIK_EXPORT bool register_model_pipeline(ModelDB *db, u64 id, ModelPipelineNode *pipeline,
                                             PFN_destroy_model_pipeline destroy = nullptr);
    AUIK_EXPORT void destroy_model_fields(Model *model);
    AUIK_EXPORT ModelID make_generated_model_id();

    template <class T>
    inline Model *make_value_model(ModelDB *db, ModelID model_id = 0u, ModelFieldID field_id = 1u, T value = {})
    {
        if (!db) return nullptr;
        if (model_id == 0u) model_id = make_generated_model_id();
        if (find_model(db, model_id)) return nullptr;
        Model model{};
        model.make_record_id_cb = make_generated_model_record_id;

        ModelRecord record{};
        auto *field = make_model_field<T>(field_id, std::move(value));
        add_model_field(record, field);
        model.add_record(std::move(record));

        if (!register_model(db, model_id, std::move(model), destroy_model_fields))
        {
            field->release();
            return nullptr;
        }
        return find_model(db, model_id);
    }

    AUIK_EXPORT u32 get_model_pipeline_intermediate_stride(ModelPipelineNode *pipeline);
    AUIK_EXPORT void sync_model_db(ModelDB *db);
    AUIK_EXPORT void clear_model_db(ModelDB *db);
    AUIK_EXPORT bool model_pipeline_cache_active(const ModelDB *db);
    AUIK_EXPORT void begin_model_pipeline_cache(ModelDB *db);
    AUIK_EXPORT void end_model_pipeline_cache(ModelDB *db);
    AUIK_EXPORT const ModelPipelineCacheEntry *find_model_pipeline_cache_entry(const ModelDB *db,
                                                                               const ModelPipelineCacheKey &key);

    template <class T>
    inline const T *find_model_pipeline_cache_value(const ModelDB *db, const ModelPipelineCacheKey &key)
    {
        const auto *entry = find_model_pipeline_cache_entry(db, key);
        return entry && entry->data && entry->size == sizeof(T) ? static_cast<const T *>(entry->data) : nullptr;
    }

    template <class T>
    inline void store_model_pipeline_cache_value(ModelDB *db, const ModelPipelineCacheKey &key, const T &value)
    {
        if (!model_pipeline_cache_active(db) || find_model_pipeline_cache_entry(db, key)) return;
        auto *data = acul::alloc<T>(value);
        if (!data) return;
        db->_pipeline_cache_entries.push_back(ModelPipelineCacheEntry{
            key,
            data,
            sizeof(T),
            [](void *ptr) { acul::release(static_cast<T *>(ptr)); },
        });
    }
    AUIK_EXPORT bool is_model_binding_valid(const ModelBinding &binding);
    AUIK_EXPORT bool is_model_field_access_valid(ModelDB *db, ModelID model_id, const ModelFieldAccess &access);
    AUIK_EXPORT void *get_model_field_access_data(ModelDB *db, ModelID model_id, const ModelFieldAccess &access);
    AUIK_EXPORT const void *get_model_field_access_data(const ModelDB *db, ModelID model_id,
                                                        const ModelFieldAccess &access);
    AUIK_EXPORT bool write_model_field_access(ModelDB *db, ModelID model_id, const ModelFieldAccess &access,
                                              const void *src);
    AUIK_EXPORT ModelField *resolve_model_field(ModelDB *db, ModelID model_id, const ModelFieldAccess &access);
    AUIK_EXPORT const ModelField *resolve_model_field(const ModelDB *db, ModelID model_id,
                                                      const ModelFieldAccess &access);
    AUIK_EXPORT bool process_model_field_access(ModelDB *db, ModelID model_id, const ModelFieldAccess &access,
                                                void *dst);
    AUIK_EXPORT bool process_model_field_access_reverse(ModelDB *db, ModelID model_id, const ModelFieldAccess &access,
                                                        const void *src);

    template <class T>
    inline ModelValueField<T> *get_model_value_field(ModelDB *db, ModelID model_id, const ModelFieldAccess &access)
    {
        auto *field = resolve_model_field(db, model_id, access);
        return field ? static_cast<ModelValueField<T> *>(field) : nullptr;
    }

    template <class T>
    inline const ModelValueField<T> *get_model_value_field(const ModelDB *db, ModelID model_id,
                                                           const ModelFieldAccess &access)
    {
        const auto *field = resolve_model_field(db, model_id, access);
        return field ? static_cast<const ModelValueField<T> *>(field) : nullptr;
    }

    template <class T>
    inline const T *get_model_field_access_value(const ModelDB *db, ModelID model_id, const ModelFieldAccess &access)
    {
        const auto *field = resolve_model_field(db, model_id, access);
        return field ? field->template as<T>() : nullptr;
    }

    template <class T, class U>
    inline bool write_model_field_access_value(ModelDB *db, ModelID model_id, const ModelFieldAccess &access, U &&value)
    {
        if (access.pipeline)
        {
            T next(std::forward<U>(value));
            return process_model_field_access_reverse(db, model_id, access, &next);
        }
        auto *field = get_model_value_field<T>(db, model_id, access);
        if (!field) return false;
        field->value = T(std::forward<U>(value));
        return true;
    }

    template <class T, class U>
    inline bool set_model_field_access_value(ModelDB *db, ModelID model_id, const ModelFieldAccess &access, U &&value)
    {
        if (!write_model_field_access_value<T>(db, model_id, access, std::forward<U>(value))) return false;
        if (auto *field = resolve_model_field(db, model_id, access)) dispatch_model_field(*field);
        return true;
    }

    AUIK_EXPORT ModelRecordID first_model_record_id(ModelDB *db, ModelID model_id);
    AUIK_EXPORT bool default_access_model_binding_field(ModelBinding *binding, ModelRecordID record_id,
                                                        ModelFieldID field_id, ModelFieldAccess *out);
    AUIK_EXPORT bool access_model_binding_field(ModelBinding &binding, ModelRecordID record_id, ModelFieldID field_id,
                                                ModelFieldAccess &out);
    AUIK_EXPORT bool access_model_binding_field(ModelBinding &binding, ModelFieldAccess &out);
    AUIK_EXPORT void dispatch_model_field(ModelField &field);
    AUIK_EXPORT bool process_model_pipeline(ModelDB *db, ModelPipelineNode *pipeline, const void *src, void *dst);
    AUIK_EXPORT bool process_model_pipeline_reverse(ModelDB *db, ModelPipelineNode *pipeline, const void *src,
                                                    void *dst);

    template <class T>
    inline bool read_model_field_access_value(ModelDB *db, ModelID model_id, const ModelFieldAccess &access, T &out)
    {
        if (access.pipeline)
        {
            const ModelPipelineCacheKey key{model_id, access.record_id, access.field_id, access.pipeline->id};
            if (access.pipeline->id != 0u)
                if (const auto *cached = find_model_pipeline_cache_value<T>(db, key))
                {
                    out = *cached;
                    return true;
                }
            if (!process_model_field_access(db, model_id, access, &out)) return false;
            if (access.pipeline->id != 0u) store_model_pipeline_cache_value(db, key, out);
            return true;
        }
        const auto *value = get_model_field_access_value<T>(db, model_id, access);
        if (!value) return false;
        out = *value;
        return true;
    }

    template <class T>
    inline bool read_model_binding_value(ModelBinding &binding, ModelRecordID record_id, ModelFieldID field_id, T &out)
    {
        ModelFieldAccess access{};
        return access_model_binding_field(binding, record_id, field_id, access) &&
               read_model_field_access_value(binding.db, binding.model_id, access, out);
    }

    template <class T>
    inline bool read_model_binding_value(ModelBinding &binding, T &out)
    {
        ModelFieldAccess access{};
        return access_model_binding_field(binding, access) &&
               read_model_field_access_value(binding.db, binding.model_id, access, out);
    }

    template <class T, class U>
    inline bool set_model_binding_value(ModelBinding &binding, ModelRecordID record_id, ModelFieldID field_id,
                                        U &&value)
    {
        ModelFieldAccess access{};
        return access_model_binding_field(binding, record_id, field_id, access) &&
               set_model_field_access_value<T>(binding.db, binding.model_id, access, std::forward<U>(value));
    }

    template <class T, class U>
    inline bool set_model_binding_value(ModelBinding &binding, U &&value)
    {
        ModelFieldAccess access{};
        return access_model_binding_field(binding, access) &&
               set_model_field_access_value<T>(binding.db, binding.model_id, access, std::forward<U>(value));
    }

    inline ModelBinding *make_model_binding(ModelID model_id)
    {
        auto *binding = acul::alloc<ModelBinding>();
        binding->model_id = model_id;
        binding->access_field = default_access_model_binding_field;
        return binding;
    }

    inline ModelBinding *make_model_binding(ModelID model_id, ModelRecordID record_id, ModelFieldID field_id,
                                            ModelPipelineNode *pipeline = nullptr)
    {
        auto *binding = make_model_binding(model_id);
        binding->default_access = {record_id, field_id, pipeline};
        return binding;
    }

    inline ModelBinding *make_value_model_binding(ModelID model_id, ModelFieldID field_id = 1u,
                                                  ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID,
                                                  ModelPipelineNode *pipeline = nullptr)
    {
        return make_model_binding(model_id, record_id, field_id, pipeline);
    }

    AUIK_EXPORT void attach_model_binding(ModelBinding &binding);

    template <class T>
    inline void erase_model_binding_ptr(acul::vector<T *> &items, T *value)
    {
        for (size_t i = 0; i < items.size();)
        {
            if (items[i] == value) items.erase(items.begin() + i);
            else ++i;
        }
    }

    AUIK_EXPORT void detach_model_binding(ModelBinding &binding);

    AUIK_EXPORT ModelRecordID *find_model_binding_record(ModelBinding &binding, ModelRecordID record_id);

    inline void bind_model_record(ModelBinding &binding, ModelRecordID record_id)
    {
        assert(binding.db && binding.db->synced);
        binding.records.push_back(record_id);
    }

    AUIK_EXPORT void rebuild_model_binding_records(ModelBinding &binding);
    AUIK_EXPORT void default_dispatch_model_binding(ModelBinding *binding, const ModelRecordsEvent &event);
    AUIK_EXPORT void default_dispatch_model_binding_field(ModelBinding *binding, ModelRecordID record_id,
                                                          ModelFieldID field_id);

    inline void dispatch_model_binding(ModelBinding &binding, const ModelRecordsEvent &event)
    {
        assert(binding.db && binding.db->synced);
        if (binding.on_dispatch) binding.on_dispatch(&binding, event);
        else default_dispatch_model_binding(&binding, event);
    }

    inline void dispatch_model_binding_field(ModelBinding &binding, ModelRecordID record_id, ModelFieldID field_id)
    {
        assert(binding.db && binding.db->synced);
        if (binding.on_field_dispatch) binding.on_field_dispatch(&binding, record_id, field_id);
        else default_dispatch_model_binding_field(&binding, record_id, field_id);
    }

    inline void Model::dispatch_records(const ModelRecordsEvent &event)
    {
        for (auto *binding : bindings)
            if (binding) dispatch_model_binding(*binding, event);
    }

    AUIK_EXPORT ModelRecord *append_model_record(ModelDB *db, ModelID model_id, ModelRecord record);
    AUIK_EXPORT bool erase_model_record(ModelDB *db, ModelID model_id, ModelRecordID record_id,
                                        bool release_fields = false);
    AUIK_EXPORT bool move_model_record(ModelDB *db, ModelID model_id, ModelRecordID record_id, u32 to_index);
    AUIK_EXPORT void reset_model_records(ModelDB *db, ModelID model_id);
    AUIK_EXPORT bool process_model_pipeline_batch(ModelDB *db, ModelPipelineNode *pipeline, const void *src,
                                                  u32 src_stride, void *dst, u32 dst_stride, u32 count);
    AUIK_EXPORT bool process_model_binding_field_batch(ModelBinding &binding, ModelFieldID field_id,
                                                       ModelPipelineNode *pipeline, void *dst, u32 dst_stride);
} // namespace auik
