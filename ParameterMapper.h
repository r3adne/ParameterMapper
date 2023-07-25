/*******************************************************************************
    Ari Lewis-Towbes 2021
    See LICENSE in top directory for license info.

    BEGIN_JUCE_MODULE_DECLARATION
    ID: parametermapper
    vendor: Relt
    version: 0.0.1
    name: ParameterMapper
    description: a useful tool which helps connect midi CCs to parameters in JUCE plugins
    dependencies: juce_audio_basics
    minimumCppStandard: 11

    END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#define NUM_CCS 16
#define MAX_NUM_PARAMETERS 128
#define MAPPING_DELETE_POOL_SIZE 1000

#include <juce_audio_processors/juce_audio_processors.h>

#include <utility>
#include "circstack.h"

namespace ParameterMapper
{


struct [[ maybe_unused ]] ParameterMappingManager
{
    enum class Consume { ConsumeAllMessages, ConsumeMappedMessages, ConsumeNoMessages };

    Consume consume_setting = Consume::ConsumeNoMessages;

    using ParameterType = juce::AudioProcessorParameterWithID;

    struct Mapping
    {
        Mapping(ParameterType* prm, juce::NormalisableRange<float> r, int ccnum) : param(prm), range(std::move(r)), cc(ccnum) {}

        //! this could be weird, but in this context `isValid` won't really be used
        Mapping(Mapping& m)
        {
            param = m.param;
            range = m.range;
            cc = m.cc;
            isValid.store(m.isValid.load());
        }

        //! We use a raw pointer here because it references the parameter owned by the Processor
        ParameterType* param;

        juce::NormalisableRange<float> range;

        int cc;

        std::atomic<bool> isValid = false;
    };

    using pMappingType = std::atomic<Mapping*>;

    circstack<int, 512> lastChangedCC;

    int start_cc = 111;

private:

    juce::StringArray mappableParamIDs;
    std::array<pMappingType, NUM_CCS * MAX_NUM_PARAMETERS> Mappings;

    // used in the process loop
    juce::MidiMessage temp_m;
    size_t temp_cc;
    juce::NormalisableRange<float> map_from_cc;
    Mapping* temp_mapping;


public:

    explicit ParameterMappingManager(juce::StringArray& _mappableParamIDs) : lastChangedCC(-1), mappableParamIDs(_mappableParamIDs), Mappings(), temp_m(), temp_cc{0},
                                map_from_cc(0, 127, 1), temp_mapping{nullptr}
    {

        for (auto& a : Mappings)
        {
            a.store(nullptr);
        }
    }


    ~ParameterMappingManager()
    {
        for (auto& a : Mappings)
        {
            delete a.load();
        }
    }


    [[ maybe_unused ]] Mapping* getMapping(size_t cc, size_t paramoffset)
    {
        return Mappings[((cc - static_cast<size_t>(start_cc)) * MAX_NUM_PARAMETERS) + paramoffset].load();
    }
    
    // call this on the message thread
    [[ maybe_unused ]] void addParameterMapping (ParameterType* parameter_to_map, juce::NormalisableRange<float> mapping_range, size_t cc)
    {
        auto* mp = new Mapping(parameter_to_map, std::move(mapping_range), static_cast<int>(cc));

        auto idx = mappableParamIDs.indexOf(parameter_to_map->getParameterID());

        if (idx == -1)
        {
            delete mp;
            return;
        }

        Mappings[static_cast<size_t>(idx + ((cc - start_cc + 1) * MAX_NUM_PARAMETERS))].store(mp);

        mp->isValid.store(true);
    }

    [[ maybe_unused ]] void deleteMapping(size_t cc, size_t paramoffset)
    {
        deleteMapping(((cc - static_cast<size_t>(start_cc) + 1) * MAX_NUM_PARAMETERS) + paramoffset);
    }

    // realtime safe
    void deleteMapping(size_t index)
    {
        Mappings[index].load()->isValid = false;
    }

private:

    //! Call this at the beginning of your processing loop.
    inline void Process (juce::MidiBuffer& buffer) noexcept
    {
        for (auto a : buffer)
        {
            temp_m = a.getMessage();

            if (temp_m.isController())
            {
                temp_cc = static_cast<size_t>(temp_m.getControllerNumber());

                for (size_t m = (temp_cc * MAX_NUM_PARAMETERS); m != ((temp_cc + 1) * MAX_NUM_PARAMETERS); ++m)
                {
                    temp_mapping = Mappings[m].load();

                    if (temp_mapping->param != nullptr && temp_mapping->isValid)
                    {
                        temp_mapping->param->beginChangeGesture();
                        temp_mapping->param->setValueNotifyingHost( temp_mapping->range.convertTo0to1(
                                                                                    map_from_cc.convertTo0to1(
                                                                                            static_cast<float>(temp_m.getControllerValue()))));
                        temp_mapping->param->endChangeGesture();
                    }
                }
            }
        }
    }
    };

} // namespace ParameterMapper