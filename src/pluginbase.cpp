// Copyright (c) 2022-2026 Manuel Schneider

#include "applicationbase.h"
#include "pluginbase.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QSettings>
#include <albert/indexitem.h>
#include <albert/logging.h>
#include <albert/widgetsutil.h>
ALBERT_LOGGING_CATEGORY("apps")
using namespace Qt::StringLiterals;
using namespace albert;
using namespace std;

static const auto ck_use_non_localized_name = "use_non_localized_name";
static const auto ck_split_camel_case = "split_camel_case";
static const auto ck_use_acronyms = "use_acronyms";

QString PluginBase::defaultTrigger() const { return u"apps "_s; }

void PluginBase::updateIndexItems()  { indexer.run(); }

void PluginBase::commonInitialize(const QSettings &s)
{
    use_non_localized_name_ = s.value(ck_use_non_localized_name, false).value<bool>();
    split_camel_case_       = s.value(ck_split_camel_case, false).value<bool>();
    use_acronyms_           = s.value(ck_use_acronyms, false).value<bool>();
}

void PluginBase::addBaseConfig(QFormLayout *l)
{
    auto *cb = new QCheckBox;
    l->addRow(tr("Use non-localized name"), cb);
    bindWidget(cb, this, &PluginBase::useNonLocalizedName, &PluginBase::setUseNonLocalizedName);

    cb = new QCheckBox;
    l->addRow(tr("Split CamelCase words (medial capital)"), cb);
    bindWidget(cb, this, &PluginBase::splitCamelCase, &PluginBase::setSplitCamelCase);

    cb = new QCheckBox;
    l->addRow(tr("Use acronyms"), cb);
    bindWidget(cb, this, &PluginBase::useAcronyms, &PluginBase::setUseAcronyms);
}

vector<IndexItem> PluginBase::buildIndexItems() const
{
    vector<IndexItem> r;

    for (const auto &iapp : applications)
    {
        auto app = static_pointer_cast<ApplicationBase>(iapp);
        for (const auto &name : app->names())
        {
            r.emplace_back(app, name);

            // https://en.wikipedia.org/wiki/Combining_Diacritical_Marks
            static QRegularExpression re(uR"([\x{0300}-\x{036f}])"_s);
            auto normalized = name.normalized(QString::NormalizationForm_D).remove(re);

            auto ccs = camelCaseSplit(normalized);

            if (split_camel_case_)
                r.emplace_back(app, ccs.join(QChar::Space));

            if (use_acronyms_)
            {
                QString acronym;
                for (const auto &w : as_const(ccs))
                    if (w.size())
                        acronym.append(w[0]);

                if (acronym.size() > 1)
                    r.emplace_back(app, acronym);
            }
        }
    }

    return r;
}

QStringList PluginBase::camelCaseSplit(const QString &s)
{
    static QRegularExpression re(uR"([A-Z0-9]?[a-z]+|[A-Z0-9]+(?![a-z]))"_s);
    auto it = re.globalMatch(s);

    QStringList words;
    while (it.hasNext())
        words << it.next().captured();

    return words;
}

bool PluginBase::useNonLocalizedName() const { return use_non_localized_name_; }

void PluginBase::setUseNonLocalizedName(bool v)
{
    if (use_non_localized_name_ != v)
    {
        settings()->setValue(ck_use_non_localized_name, v);
        use_non_localized_name_ = v;
        updateIndexItems();
    }
}

bool PluginBase::splitCamelCase() const { return split_camel_case_; }

void PluginBase::setSplitCamelCase(bool v)
{
    if (split_camel_case_ != v)
    {
        settings()->setValue(ck_split_camel_case, v);
        split_camel_case_ = v;
        setIndexItems(buildIndexItems());
    }
}

bool PluginBase::useAcronyms() const { return use_acronyms_; }

void PluginBase::setUseAcronyms(bool v)
{
    if (use_acronyms_ != v)
    {
        settings()->setValue(ck_use_acronyms, v);
        use_acronyms_ = v;
        setIndexItems(buildIndexItems());
    }
}
