#ifndef MASTER_FILE_PARSER_H
#define MASTER_FILE_PARSER_H

#include "ContractData.h"
#include <QString>
#include <QStringView>

/**
 * @brief Utility class for parsing master contract files
 *
 * Master file formats:
 * - NSECM: Pipe-separated, ~17 fields
 * - NSEFO: Pipe-separated, ~19 fields (includes F&O specific data)
 * - BSECM: Pipe-separated, ~18 fields (includes ScripCode)
 * - BSEFO: Pipe-separated, ~19 fields (includes F&O + ScripCode)
 */
class MasterFileParser {
public:
  /**
   * @brief Parse a single line from a master file
   * @param line Raw line from file
   * @param exchange Exchange segment (NSECM, NSEFO, BSECM, BSEFO)
   * @param contract Output MasterContract
   * @return true if parsed successfully
   */
  static bool parseLine(const QStringView &line, const QString &exchange,
                        MasterContract &contract);

  /**
   * @brief Utility: Remove surrounding quotes from a string
   * @param str String reference (possibly quoted)
   * @return String without quotes
   *
   * Shared utility to avoid code duplication across repositories.
   */
  static QString trimQuotes(const QStringView &str);

private:
  /**
   * @brief Parse NSECM master line
   */
  static bool parseNSECM(const QVector<QStringView> &fields,
                         MasterContract &contract);
  static bool parseNSEFO(const QVector<QStringView> &fields,
                         MasterContract &contract);
  static bool parseBSECM(const QVector<QStringView> &fields,
                         MasterContract &contract);
  static bool parseBSEFO(const QVector<QStringView> &fields,
                         MasterContract &contract);

  /**
   * @brief Helper: Convert option type int to string
   */
  static QString convertOptionType(int32_t optionType);
};

#endif // MASTER_FILE_PARSER_H
