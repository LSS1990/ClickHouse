#pragma once

#include <Poco/Timespan.h>
#include <DB/Core/Defines.h>
#include <DB/Core/Field.h>
#include <DB/Interpreters/SettingsCommon.h>


namespace DB
{

/** Ограничения при выполнении запроса - часть настроек.
  * Используются, чтобы обеспечить более безопасное исполнение запросов из пользовательского интерфейса.
  * В основном, ограничения проверяются на каждый блок (а не на каждую строку). То есть, ограничения могут быть немного нарушены.
  * Почти все ограничения действуют только на SELECT-ы.
  * Почти все ограничения действуют на каждый поток по отдельности.
  */
struct Limits
{
	/** Перечисление ограничений: тип, имя, значение по-умолчанию.
	  * По-умолчанию: всё не ограничено, кроме довольно слабых ограничений на глубину рекурсии и размер выражений.
	  */

#define APPLY_FOR_SETTINGS(M) \
	/** Ограничения на чтение из самых "глубоких" источников. \
	  * То есть, только в самом глубоком подзапросе. \
	  * При чтении с удалённого сервера, проверяется только на удалённом сервере. \
	  */ \
	M(SettingUInt64, max_rows_to_read, 0) \
	M(SettingUInt64, max_bytes_to_read, 0) \
	M(SettingOverflowMode<false>, read_overflow_mode, OverflowMode::THROW) \
	\
	M(SettingUInt64, max_rows_to_group_by, 0) \
	M(SettingOverflowMode<true>, group_by_overflow_mode, OverflowMode::THROW) \
	\
	M(SettingUInt64, max_rows_to_sort, 0) \
	M(SettingUInt64, max_bytes_to_sort, 0) \
	M(SettingOverflowMode<false>, sort_overflow_mode, OverflowMode::THROW) \
	\
	/** Ограничение на размер результата. \
	  * Проверяются также для подзапросов и на удалённых серверах. \
	  */ \
	M(SettingUInt64, max_result_rows, 0) \
	M(SettingUInt64, max_result_bytes, 0) \
	M(SettingOverflowMode<false>, result_overflow_mode, OverflowMode::THROW) \
	\
	/* TODO: Проверять также при merge стадии сортировки, при слиянии и финализации агрегатных функций. */ \
	M(SettingSeconds, max_execution_time, 0) \
	M(SettingOverflowMode<false>, timeout_overflow_mode, OverflowMode::THROW) \
	\
	/** В строчках в секунду. */ \
	M(SettingUInt64, min_execution_speed, 0) \
	/** Проверять, что скорость не слишком низкая, после прошествия указанного времени. */ \
	M(SettingSeconds, timeout_before_checking_execution_speed, 0) \
	\
	M(SettingUInt64, max_columns_to_read, 0) \
	M(SettingUInt64, max_temporary_columns, 0) \
	M(SettingUInt64, max_temporary_non_const_columns, 0) \
	\
	M(SettingUInt64, max_subquery_depth, 100) \
	M(SettingUInt64, max_pipeline_depth, 1000) \
	M(SettingUInt64, max_ast_depth, 1000)		/** Проверяются не во время парсинга, */ \
	M(SettingUInt64, max_ast_elements, 10000)	/**  а уже после парсинга запроса. */ \
	\
	M(SettingBool, readonly, false) \
	\
	/** Ограничения для максимального размера множества, получающегося при выполнении секции IN. */ \
	M(SettingUInt64, max_rows_in_set, 0) \
	M(SettingUInt64, max_bytes_in_set, 0) \
	M(SettingOverflowMode<false>, set_overflow_mode, OverflowMode::THROW) \
	\
	/** Ограничения для максимального размера запоминаемого состояния при выполнении DISTINCT. */ \
	M(SettingUInt64, max_rows_in_distinct, 0) \
	M(SettingUInt64, max_bytes_in_distinct, 0) \
	M(SettingOverflowMode<false>, distinct_overflow_mode, OverflowMode::THROW)

#define DECLARE(TYPE, NAME, DEFAULT) \
	TYPE NAME {DEFAULT};

	APPLY_FOR_SETTINGS(DECLARE)

#undef DECLARE

	/// Установить настройку по имени.
	bool trySet(const String & name, const Field & value)
	{
	#define TRY_SET(TYPE, NAME, DEFAULT) \
		else if (name == #NAME) NAME.set(value);

		if (false) {}
		APPLY_FOR_SETTINGS(TRY_SET)
		else
			return false;

		return true;

	#undef TRY_SET
	}

	/// Установить настройку по имени. Прочитать сериализованное в бинарном виде значение из буфера (для межсерверного взаимодействия).
	bool trySet(const String & name, ReadBuffer & buf)
	{
	#define TRY_SET(TYPE, NAME, DEFAULT) \
		else if (name == #NAME) NAME.set(buf);

		if (false) {}
		APPLY_FOR_SETTINGS(TRY_SET)
		else
			return false;

		return true;

	#undef TRY_SET
	}

	/** Установить настройку по имени. Прочитать значение в текстовом виде из строки (например, из конфига, или из параметра URL).
	  */
	bool trySet(const String & name, const String & value)
	{
	#define TRY_SET(TYPE, NAME, DEFAULT) \
		else if (name == #NAME) NAME.set(value);

		if (false) {}
		APPLY_FOR_SETTINGS(TRY_SET)
		else
			return false;

		return true;

	#undef TRY_SET
	}

private:
	friend struct Settings;
	
	/// Записать все настройки в буфер. (В отличие от соответствующего метода в Settings, пустая строка на конце не пишется).
	void serialize(WriteBuffer & buf) const
	{
	#define WRITE(TYPE, NAME, DEFAULT) \
		if (NAME.changed) \
		{ \
			writeStringBinary(#NAME, buf); \
			NAME.write(buf); \
		}

		APPLY_FOR_SETTINGS(WRITE)

	#undef WRITE
	}

#undef APPLY_FOR_SETTINGS
};


}
