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
        //! We use a raw pointer here because it references the parameter owned by the Processor
        ParameterType* param;

        juce::NormalisableRange<float> range;

        int cc;

        bool isValid = false;
    };

    using pMappingType = std::atomic<Mapping*>;

    circstack<int, 512> lastChangedCC;

private:

    std::array<pMappingType, NUM_CCS * MAX_NUM_PARAMETERS> Mappings;

    //! This is just a pool of pointers pointing to mappings we need to delete on the message thread, as deleting them on the audio thread isn't safe.
    std::array<size_t, MAPPING_DELETE_POOL_SIZE> DeletePool;
    size_t deletePoolWrite = 0, deletePoolRead = 0;

    // used in the process loop
    juce::MidiMessage temp_m;
    int temp_cc, temp_ch;
    juce::NormalisableRange<float> map_from_cc;
    Mapping temp_mapping;


public:

    ParameterMappingManager() : lastChangedCC(-1), Mappings(), DeletePool(), temp_m(), temp_cc{-1}, temp_ch{-1},
                                map_from_cc(0, 127, 1), temp_mapping{nullptr, {0, 127}, -1}
    {
        for (auto& a : Mappings)
        {
            a.store(nullptr);
        }
    }


    ~ParameterMappingManager()
    {
        removeAll();

        for (auto& a : Mappings)
        {
            delete a.load();
        }
    }


//    // probably not necessary
//    std::array<Mapping, MAX_NUM_PARAMETERS> getMappingsForCC(int cc)
//    {
//        std::array<pMappingType, MAX_NUM_PARAMETERS> op;
//
//        std::copy(Mappings.begin() + MAX_NUM_PARAMETERS * cc, (Mappings.begin() + MAX_NUM_PARAMETERS * cc) + MAX_NUM_PARAMETERS, op.begin());
//        return op;
//    }

    Mapping getMapping(int cc, int paramoffset)
    {
        return std::move(*(Mappings.begin() + cc * paramoffset)->load());
    }


    //! returns an array of all mappings... note that the pointers (param, and more likely range, can be invalidated)
    std::array<Mapping, MAX_MAPPINGS> getAllMappings()
    {
        std::array<Mapping, MAX_MAPPINGS> op;

        for (size_t i = 0; i < op.size(); ++i)
        {
            op[i] = Mappings[i].load();
        }

        return op;
    }

private:

    MappingPairType* getFirstNullMapping()
    {
        for (auto a = Mappings.begin(); a != Mappings.end(); ++a)
        {
            if (! a->load().param)
            {
                return a;
            }
        }

        return nullptr;
    }


    // call this on the message thread
    [[ maybe_unused ]] void addParameterMapping (ParameterType* parameter_to_map, juce::NormalisableRange<float> mapping_range, int cc, int channel)
    {
        jassert(juce::isPositiveAndBelow(cc+1, 128));
        jassert(juce::isPositiveAndBelow(channel, 16));

        auto m = getFirstNullMapping();

        // deleted upon removeParameterMapping or when this manager is deleted
        auto* range = new juce::NormalisableRange<float>(std::move(mapping_range));

        auto map = Mapping{parameter_to_map, range, cc, channel};

        m->store(map);
    }


    // realtime safe
    void markForRemoval(MappingPairType* m)
    {
        deletePoolWrite = m;
        ++deletePoolWrite;
        if (deletePoolWrite == DeletePool.end()) deletePoolWrite = DeletePool.begin();
        // note that overflows here aren't thrown or asserted, just don't let that happen :3
    }


    // not realtime safe!
    int removeAll()
    {
        int count = 0;
        while (deletePoolRead != deletePoolWrite)
        {
            delete deletePoolRead->load().range;
            delete deletePoolRead;
            ++deletePoolRead;
            if (deletePoolRead == DeletePool.end()) deletePoolRead = DeletePool.begin();
            ++count;
        }
        return count;
    }


    // todo: update removeParameterMappings
    //! Removes all parameter mappings for a given parameter on a given cc and channel
    template <bool is_on_realtime_thread>
    [[ maybe_unused ]] void removeParameterMappings(ParameterType* parameter_to_map, int cc, int channel)
    {
        for (auto a = Mappings.begin() + (cc * channel); a != Mappings.begin() + (cc * channel) + MAX_MAPPINGS_PER_PARAMETER; ++a)
        {
            if (a->load().param == parameter_to_map)
            {
                markForRemoval(a);

                if (! is_on_realtime_thread) removeAll();
            }
        }
    }

//    todo: update removeParameterMappings
    //! Removes all parameter mappings for a given cc and channel
    template <bool is_on_realtime_thread>
    [[ maybe_unused ]] void removeParameterMappings(int cc, int channel)
    {
        for (auto a = Mappings.begin() + (cc * channel); a != Mappings.begin() + (cc * channel) + MAX_MAPPINGS_PER_PARAMETER; ++a)
        {
            markForRemoval(a);

            if (! is_on_realtime_thread) removeAll();
        }
    }

//    todo: update Process
    //! Call this at the beginning of your processing loop.
    inline void Process (juce::MidiBuffer& buffer) noexcept
    {
        for (auto a : buffer)
        {
            temp_m = a.getMessage();

            if (temp_m.isController())
            {
                temp_cc = temp_m.getControllerNumber();
                temp_ch = temp_m.getChannel();

                for (auto m = Mappings.begin() + (temp_cc * temp_ch); m != Mappings.begin() + (temp_cc * temp_ch) + MAX_MAPPINGS_PER_PARAMETER; ++m)
                {
                    temp_mapping = m->load();
                    if (temp_mapping.param != nullptr)
                    {
                        temp_mapping.param->beginChangeGesture();
                        temp_mapping.param->setValueNotifyingHost( temp_mapping.range->convertTo0to1(
                                                                                    map_from_cc.convertTo0to1(
                                                                                            static_cast<float>(temp_m.getControllerValue()))));
                        temp_mapping.param->endChangeGesture();
                    }
                }

                if (lastChangedCCChannelPair.top().first != temp_cc)
                {
                    lastChangedCCChannelPair.push(std::make_pair(temp_cc, temp_ch));
                }
            }
        }
    }
    };

} // namespace ParameterMapper