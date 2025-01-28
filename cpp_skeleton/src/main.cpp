#include <skeleton/actions.h>
#include <skeleton/constants.h>
#include <skeleton/runner.h>
#include <skeleton/states.h>
#include <iostream>
#include <array>
#include <time.h>
#include <cmath>

#include <utility>

#include <skeleton/poker.h>
#include <skeleton/arrays.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cstdint>

inline int rankOfCard(std::uint32_t card)
{
    return static_cast<int>((card >> 8) & 0xF);
}

class Card
{
public:
    std::uint32_t code;

    Card() : code(0) {}
    explicit Card(std::uint32_t c) : code(c) {}

    // Rank index: 0..12 (Deuce..Ace)
    int rank() const
    {
        return rankOfCard(code);
    }

    //   0x8000 -> clubs, 0x4000 -> diamonds, 0x2000 -> hearts, 0x1000 -> spades
    char suitChar() const
    {
        if (code & 0x8000)
            return 'c';
        else if (code & 0x4000)
            return 'd';
        else if (code & 0x2000)
            return 'h';
        else
            return 's';
    }

    // Convert rank index to char representation: 2..9, T, J, Q, K, A
    char rankChar() const
    {
        static const char rankChars[] = {'2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K', 'A'};
        return rankChars[rank()];
    }

    void print() const
    {
        std::cout << rankChar() << suitChar();
    }
};

class Deck
{
private:
    std::vector<Card> cards_;
    std::mt19937 rng_;

public:
    Deck()
    {
        std::random_device rd;
        rng_ = std::mt19937(rd());
        init();
    }

    void init()
    {
        cards_.resize(52);

        // suits in original code: clubs=0x8000, diamonds=0x4000,
        //                        hearts=0x2000, spades=0x1000
        static const std::uint32_t suits[4] = {0x8000, 0x4000, 0x2000, 0x1000};

        int index = 0;
        for (int s = 0; s < 4; ++s)
        {
            for (int r = 0; r < 13; ++r)
            {
                // prime = primes[r] (lowest 8 bits)
                // rank bits [11..8] = r
                // suit bits [15..12] = suits[s]
                // plus rank bit mask at bit [16 + r]
                std::uint32_t primePart = (primes[r] & 0xFF);
                std::uint32_t rankPart = (r << 8) & 0xF00;
                std::uint32_t suitPart = suits[s] & 0xF000;
                std::uint32_t rankMask = (1 << (16 + r));
                std::uint32_t code = primePart | rankPart | suitPart | rankMask;

                cards_[index++] = Card(code);
            }
        }
    }

    // Shuffle using <algorithm>'s std::shuffle
    void shuffle()
    {
        std::shuffle(cards_.begin(), cards_.end(), rng_);
    }

    // Access
    Card &operator[](std::size_t i) { return cards_[i]; }
    const Card &operator[](std::size_t i) const { return cards_[i]; }

    std::size_t size() const { return cards_.size(); }
};

inline unsigned fastHash(unsigned u)
{
    u += 0xe91aaa35;
    u ^= (u >> 16);
    u += (u << 8);
    u ^= (u >> 4);
    unsigned b = (u >> 8) & 0x1ff;
    unsigned a = (u + (u << 2)) >> 19;
    return a ^ hash_adjust[b];
}

unsigned short eval5Hand(const Card &c1, const Card &c2, const Card &c3, const Card &c4, const Card &c5)
{
    // Combine bits
    std::uint32_t orAll = c1.code | c2.code | c3.code | c4.code | c5.code;
    int idx = static_cast<int>(orAll >> 16);

    // Check if all suits are the same
    bool allSameSuit = ((c1.code & c2.code & c3.code & c4.code & c5.code & 0xF000) != 0);

    // flushes[] table lookup
    if (allSameSuit)
    {
        unsigned short flushVal = flushes[idx];
        if (flushVal != 0)
        {
            return flushVal; // flush or straight-flush
        }
    }

    // unique5[] table lookup (for straights or high card)
    unsigned short maybe = unique5[idx];
    if (maybe != 0)
    {
        return maybe; // straight or high card
    }

    // Otherwise, do prime-product perfect-hash approach
    unsigned product = (c1.code & 0xFF) * (c2.code & 0xFF) * (c3.code & 0xFF) * (c4.code & 0xFF) * (c5.code & 0xFF);

    unsigned r = fastHash(product);
    return hash_values[r];
}

int handRank(unsigned short val)
{
    if (val > 6185)
        return HIGH_CARD; // 1277 high card
    if (val > 3325)
        return ONE_PAIR; // 2860 one pair
    if (val > 2467)
        return TWO_PAIR; //  858 two pair
    if (val > 1609)
        return THREE_OF_A_KIND; //  858 three-kind
    if (val > 1599)
        return STRAIGHT; //   10 straights
    if (val > 322)
        return FLUSH; // 1277 flushes
    if (val > 166)
        return FULL_HOUSE; //  156 full house
    if (val > 10)
        return FOUR_OF_A_KIND; //  156 four-kind
    return STRAIGHT_FLUSH;     //   10 straight-flushes
}

// For printing final classification
static const char *HAND_TYPE_NAME[] = {
    "",
    "Straight Flush",
    "Four of a Kind",
    "Full House",
    "Flush",
    "Straight",
    "Three of a Kind",
    "Two Pair",
    "One Pair",
    "High Card"};

// GENERATE CARD CODE

std::uint32_t generateCardCode(int rankIndex, int suitIndex)
{
    // Suits encoding (0x8000 = clubs, 0x4000 = diamonds, 0x2000 = hearts, 0x1000 = spades)

    // T is the 8th rank (0 = 2, 1 = 3, ..., 8 = T, ..., 12 = A)
    // Hearts is the 3rd suit (0 = clubs, 1 = diamonds, 2 = hearts, 3 = spades)

    static const std::uint32_t suits[] = {0x8000, 0x4000, 0x2000, 0x1000};

    // Ensure valid inputs
    if (rankIndex < 0 || rankIndex > 12 || suitIndex < 0 || suitIndex > 3)
    {
        throw std::invalid_argument("Invalid rank or suit index");
    }

    // Generate the 32-bit card code
    std::uint32_t primePart = (primes[rankIndex] & 0xFF); // Prime part for the rank
    std::uint32_t rankPart = (rankIndex << 8) & 0xF00;    // Rank bits [11..8]
    std::uint32_t suitPart = suits[suitIndex] & 0xF000;   // Suit bits [15..12]
    std::uint32_t rankMask = (1 << (16 + rankIndex));     // Rank mask
    return primePart | rankPart | suitPart | rankMask;
}

auto generateCardCodeFromString = [](const std::string &cardStr) -> std::uint32_t
{
    std::unordered_map<char, int> rankMap = {{'2', 0}, {'3', 1}, {'4', 2}, {'5', 3}, {'6', 4}, {'7', 5}, {'8', 6}, {'9', 7}, {'T', 8}, {'J', 9}, {'Q', 10}, {'K', 11}, {'A', 12}};
    std::unordered_map<char, int> suitMap = {{'c', 0}, {'d', 1}, {'h', 2}, {'s', 3}};
    int rankIndex = rankMap[cardStr[0]];
    int suitIndex = suitMap[cardStr[1]];
    return generateCardCode(rankIndex, suitIndex);
};

struct BestHandResult
{
    unsigned short minVal;
    std::vector<Card> bestCombination;
};

BestHandResult evalHand(const std::vector<Card> &cards)
{
    if (cards.size() < 5 || cards.size() > 7)
    {
        throw std::invalid_argument("Number of cards must be between 5 and 7.");
    }

    BestHandResult result;
    result.minVal = 8000;
    result.bestCombination = {};

    size_t n = cards.size();

    for (size_t i = 0; i < n - 4; ++i)
    {
        for (size_t j = i + 1; j < n - 3; ++j)
        {
            for (size_t k = j + 1; k < n - 2; ++k)
            {
                for (size_t l = k + 1; l < n - 1; ++l)
                {
                    for (size_t m = l + 1; m < n; ++m)
                    {
                        unsigned short val = eval5Hand(cards[i], cards[j], cards[k], cards[l], cards[m]);

                        if (val < result.minVal)
                        {
                            result.minVal = val;
                            result.bestCombination = {cards[i], cards[j], cards[k], cards[l], cards[m]};
                        }
                    }
                }
            }
        }
    }

    return result; // Return the best hand result
}

// BOUNTY HYPERPARAMETERS
static const int roundsPerBounty = 25;
static const double bountyRatio = 1.5;
static const int bountyConstant = 10;

using namespace pokerbots::skeleton;

// GAME PARAMETERS
static const int numRounds = NUM_ROUNDS;
static const int startingStack = STARTING_STACK;
static const int bigBlindVal = BIG_BLIND;
static const int smallBlindVal = SMALL_BLIND;

struct Bot
{

    int totalRounds = 1;
    int timesBetPreflop = 0;

    Deck deckInstance;

    bool alreadyWon = false;

    int numMCTrials = 600;

    int numOppChecks = 0;
    int numSelfChecks = 0;
    int oppLastContribution = 0;

    double raiseFactor = 0.05;
    double reRaiseFactor = 0.02;

    bool hasBounty = false;
    int bountyRaises = 0;
    bool alarmBell = false;

    bool nitToggle = true; // think reversely
    bool looseToggle = false;

    int oppRaiseAsDealer = 0;
    int oppReraiseAsBB = 0;
    int ourRaiseAsDealer = 0;

    bool oppReRaiseAsBBMore = true;
    bool oppRaiseAsDealerLess = true;

    bool twoCheckBluff = false;
    int pmTwoCheckBluff = 0;
    bool threeCheckBluff = false;
    int pmThreeCheckBluff = 0;
    bool bountyBluff = false;
    int pmBountyBluff = 0;

    bool permanentNoTwoCheck = false;
    int twoCheckBluffCounter = 0;
    bool permanentNoThreeCheck = false;
    int threeCheckBluffCounter = 0;
    bool permanentNoBountyBluff = false;
    int bountyBluffCounter = 0;

    bool oppBetLastRound = false;

    int numOppBetNoCheck = 0;
    int totalOppChecks = 0;
    int numOppBets = 0;
    int numOppPotBets = 0;

    int unnitBigBetFact = 0;
    int bluffCatcherFact = 0;
    int oppReraiseFact = 0;

    int oppNumReraise = 0;
    int oppNumBetsThisRound = 0;
    int ourRaisesThisRound = 0;
    int ourTotalRaises = 0;
    int oppTotalReraises = 0;

    double alreadyWonConst = 0.25;

    int lastStreet = -1;

    bool aggressiveMode = false;

    int consecutivePassive = 0;
    bool oppCheckFold = false;

    std::unordered_map<std::string, int> regularPreflopDict = {
        {"AAo", 1}, {"KKo", 2}, {"QQo", 3}, {"JJo", 4}, {"TTo", 5}, {"99o", 10}, {"88o", 10}, {"AKs", 6}, {"77o", 11}, {"AQs", 9}, {"AJs", 11}, 
        {"AKo", 6}, {"ATs", 13}, {"AQo", 14}, {"AJo", 15}, {"KQs", 16}, {"KJs", 17}, {"A9s", 18}, {"ATo", 19}, {"66o", 20}, {"A8s", 21}, {"KTs", 22}, 
        {"KQo", 23}, {"A7s", 24}, {"A9o", 25}, {"KJo", 26}, {"55o", 27}, {"QJs", 28}, {"K9s", 29}, {"A5s", 30}, {"A6s", 31}, {"A8o", 32}, {"KTo", 33}, 
        {"QTs", 34}, {"A4s", 35}, {"A7o", 36}, {"K8s", 37}, {"A3s", 38}, {"QJo", 39}, {"K9o", 40}, {"A5o", 41}, {"A6o", 42}, {"Q9s", 43}, {"K7s", 44}, 
        {"JTs", 45}, {"A2s", 46}, {"QTo", 47}, {"44o", 48}, {"A4o", 49}, {"K6s", 50}, {"K8o", 51}, {"Q8s", 52}, {"A3o", 53}, {"K5s", 54}, {"J9s", 55}, 
        {"Q9o", 56}, {"JTo", 57}, {"K7o", 58}, {"A2o", 59}, {"K4s", 60}, {"Q7s", 61}, {"K6o", 62}, {"K3s", 63}, {"T9s", 64}, {"J8s", 65}, {"33o", 66}, 
        {"Q6s", 67}, {"Q8o", 68}, {"K5o", 69}, {"J9o", 70}, {"K2s", 71}, {"Q5s", 72}, {"T8s", 73}, {"K4o", 74}, {"J7s", 75}, {"Q4s", 76}, {"Q7o", 77}, 
        {"T9o", 78}, {"J8o", 79}, {"K3o", 80}, {"Q6o", 81}, {"Q3s", 82}, {"98s", 83}, {"T7s", 84}, {"J6s", 85}, {"K2o", 86}, {"22o", 87}, {"Q2s", 87}, 
        {"Q5o", 89}, {"J5s", 90}, {"T8o", 91}, {"J7o", 92}, {"Q4o", 93}, {"97s", 80}, {"J4s", 95}, {"T6s", 96}, {"J3s", 97}, {"Q3o", 98}, {"98o", 99}, 
        {"87s", 85}, {"T7o", 101}, {"J6o", 102}, {"96s", 103}, {"J2s", 104}, {"Q2o", 105}, {"T5s", 106}, {"J5o", 107}, {"T4s", 108}, {"97o", 109}, 
        {"86s", 110}, {"J4o", 111}, {"T6o", 112}, {"95s", 113}, {"T3s", 114}, {"76s", 90}, {"J3o", 116}, {"87o", 117}, {"T2s", 118}, {"85s", 119}, 
        {"96o", 120}, {"J2o", 121}, {"T5o", 122}, {"94s", 123}, {"75s", 124}, {"T4o", 125}, {"93s", 126}, {"86o", 127}, {"65s", 128}, {"84s", 129}, 
        {"95o", 130}, {"53s", 131}, {"92s", 132}, {"76o", 133}, {"74s", 134}, {"65o", 135}, {"54s", 130}, {"85o", 137}, {"64s", 138}, {"83s", 139}, 
        {"43s", 140}, {"75o", 141}, {"82s", 142}, {"73s", 143}, {"93o", 144}, {"T2o", 145}, {"T3o", 146}, {"63s", 147}, {"84o", 148}, {"92o", 149}, 
        {"94o", 150}, {"74o", 151}, {"72s", 152}, {"54o", 153}, {"64o", 154}, {"52s", 155}, {"62s", 156}, {"83o", 157}, {"42s", 158}, {"82o", 159}, 
        {"73o", 160}, {"53o", 161}, {"63o", 162}, {"32s", 163}, {"43o", 164}, {"72o", 165}, {"52o", 166}, {"62o", 167}, {"42o", 168}, {"32o", 169}};

    std::unordered_map<std::string, int> aggPreflopDict = {
        {"AAo", 1}, {"KKo", 2}, {"QQo", 3}, {"JJo", 4}, {"TTo", 5}, {"99o", 10}, {"88o", 10}, {"AKs", 6}, {"77o", 11}, {"AQs", 9}, {"AJs", 11}, 
        {"AKo", 6}, {"ATs", 13}, {"AQo", 14}, {"AJo", 15}, {"KQs", 16}, {"KJs", 27}, {"A9s", 18}, {"ATo", 19}, {"66o", 20}, {"A8s", 21}, {"KTs", 27}, 
        {"KQo", 23}, {"A7s", 24}, {"A9o", 25}, {"KJo", 27}, {"55o", 25}, {"QJs", 27}, {"K9s", 25}, {"A5s", 25}, {"A6s", 25}, {"A8o", 25}, {"KTo", 25}, 
        {"QTs", 34}, {"A4s", 25}, {"A7o", 25}, {"K8s", 37}, {"A3s", 25}, {"QJo", 39}, {"K9o", 40}, {"A5o", 25}, {"A6o", 25}, {"Q9s", 43}, {"K7s", 44}, 
        {"JTs", 45}, {"A2s", 25}, {"QTo", 47}, {"44o", 25}, {"A4o", 25}, {"K6s", 50}, {"K8o", 51}, {"Q8s", 52}, {"A3o", 25}, {"K5s", 54}, {"J9s", 55}, 
        {"Q9o", 56}, {"JTo", 57}, {"K7o", 58}, {"A2o", 25}, {"K4s", 60}, {"Q7s", 61}, {"K6o", 62}, {"K3s", 63}, {"T9s", 64}, {"J8s", 65}, {"33o", 25}, 
        {"Q6s", 67}, {"Q8o", 68}, {"K5o", 69}, {"J9o", 70}, {"K2s", 71}, {"Q5s", 72}, {"T8s", 73}, {"K4o", 74}, {"J7s", 75}, {"Q4s", 76}, {"Q7o", 77}, 
        {"T9o", 78}, {"J8o", 79}, {"K3o", 80}, {"Q6o", 81}, {"Q3s", 82}, {"98s", 83}, {"T7s", 84}, {"J6s", 85}, {"K2o", 86}, {"22o", 25}, {"Q2s", 87}, 
        {"Q5o", 89}, {"J5s", 90}, {"T8o", 91}, {"J7o", 92}, {"Q4o", 93}, {"97s", 80}, {"J4s", 95}, {"T6s", 96}, {"J3s", 97}, {"Q3o", 98}, {"98o", 99}, 
        {"87s", 85}, {"T7o", 101}, {"J6o", 102}, {"96s", 103}, {"J2s", 104}, {"Q2o", 105}, {"T5s", 106}, {"J5o", 107}, {"T4s", 108}, {"97o", 109}, 
        {"86s", 110}, {"J4o", 111}, {"T6o", 112}, {"95s", 113}, {"T3s", 114}, {"76s", 90}, {"J3o", 116}, {"87o", 117}, {"T2s", 118}, {"85s", 119}, 
        {"96o", 120}, {"J2o", 121}, {"T5o", 122}, {"94s", 123}, {"75s", 124}, {"T4o", 125}, {"93s", 126}, {"86o", 127}, {"65s", 128}, {"84s", 129}, 
        {"95o", 130}, {"53s", 131}, {"92s", 132}, {"76o", 133}, {"74s", 134}, {"65o", 135}, {"54s", 130}, {"85o", 137}, {"64s", 138}, {"83s", 139}, 
        {"43s", 140}, {"75o", 141}, {"82s", 142}, {"73s", 143}, {"93o", 144}, {"T2o", 145}, {"T3o", 146}, {"63s", 147}, {"84o", 148}, {"92o", 149}, 
        {"94o", 150}, {"74o", 151}, {"72s", 152}, {"54o", 153}, {"64o", 154}, {"52s", 155}, {"62s", 156}, {"83o", 157}, {"42s", 158}, {"82o", 159}, 
        {"73o", 160}, {"53o", 161}, {"63o", 162}, {"32s", 163}, {"43o", 164}, {"72o", 165}, {"52o", 166}, {"62o", 167}, {"42o", 168}, {"32o", 169}};


    /*
      Called when a new round starts. Called NUM_ROUNDS times.

      @param gameState The GameState object.
      @param roundState The RoundState object.
      @param active Your player's index.
    */
    void handleNewRound(GameInfoPtr gameState, RoundStatePtr roundState, int active)
    {
        int myBankroll = gameState->bankroll;     // the total number of chips you've gained or lost from the beginning of the game to the start of this round
        float gameClock = gameState->gameClock;   // the total number of seconds your bot has left to play this game
        int roundNum = gameState->roundNum;       // the round number from 1 to State.NUM_ROUNDS
        auto myCards = roundState->hands[active]; // your cards
        bool bigBlind = (active == 1);            // true if you are the big blind

        timesBetPreflop = 0;
        oppNumReraise = 0;
        oppNumBetsThisRound = 0;
        ourRaisesThisRound = 0;

        nitToggle = (myBankroll > 1750) ? false : true;
        if (!nitToggle)
        {
            std::cout << "Nit toggle set to FALSE " << nitToggle << std::endl;
        }


        if (gameClock < 30)
        {
            std::cout << "Time is out to 30" << std::endl;
            numMCTrials = 400;
        }
        else if (gameClock < 20){
            std::cout << "Time is out to 20" << std::endl;
            numMCTrials = 250;
        }
        else if (gameClock < 10){
            std::cout << "Time is out to 10" << std::endl;
            numMCTrials = 150;
        }

        hasBounty = false;
        bountyRaises = 0;
        alarmBell = false;
        if (myBankroll > 1000)
        {
            bountyRaises++;
            oppReRaiseAsBBMore = true;
            oppRaiseAsDealerLess = true;
            //std::cout << "No more bounty bluff raises" << std::endl;
        }


        numOppChecks = 0;
        numSelfChecks = 0;
        oppLastContribution = 0;

        twoCheckBluff = false;
        threeCheckBluff = false;
        bountyBluff = false;

        oppBetLastRound = false;

        int remainingRounds = numRounds - roundNum + 1;

        double standardDeviation = pow(remainingRounds*0.15*0.85, 0.5);
        
        double alreadyWonNumOppBountyThreshold = remainingRounds*0.15 + 3.6969*standardDeviation;
        
        alreadyWonConst = alreadyWonNumOppBountyThreshold / remainingRounds;
        
        double alreadyWonBankrollThreshold = 1.5 * remainingRounds + bountyConstant * remainingRounds * alreadyWonConst + 53;
        
        int roundedAlreadyWonBankrollThreshold = (int)ceil(alreadyWonBankrollThreshold);


        double aggNumOppBountyThreshold = remainingRounds*0.15 + 1*standardDeviation;
        
        double aggConst = aggNumOppBountyThreshold / remainingRounds;
        
        double aggBankrollThreshold = 1.5 * remainingRounds + bountyConstant * remainingRounds * aggConst + 53;
        
        int roundedAggBankrollThreshold = (int)ceil(aggBankrollThreshold * 0.7); // mult by 0.6 because otherwise it is too high
        
        //std::cout << "agg threshold: " << roundedAggBankrollThreshold << std::endl;
        //std::cout << "alreadyWon threshold: " << roundedAlreadyWonBankrollThreshold << std::endl;

        std::cout << "\n#" << totalRounds << std::endl;
        if (myBankroll > roundedAlreadyWonBankrollThreshold)
        {
            alreadyWon = true;
            std::cout << "Already won: YIPPEE!" << std::endl;
        }
        
        if (myBankroll < -1 * roundedAggBankrollThreshold && roundNum > 299)
        {
            aggressiveMode = true;
            std::cout << "agg mode true" << std::endl;
        }
        else
        {
            aggressiveMode = false;
            //std::cout << "agg mode false" << std::endl;
        }

        
    }

    /*
      Called when a round ends. Called NUM_ROUNDS times.

      @param gameState The GameState object.
      @param terminalState The TerminalState object.
      @param active Your player's index.
    */
    void handleRoundOver(GameInfoPtr gameState, TerminalStatePtr terminalState, int active)
    {
        int myDelta = terminalState->deltas[active];                                                   // your bankroll change from this round
        auto previousState = std::static_pointer_cast<const RoundState>(terminalState->previousState); // RoundState before payoffs
        int street = previousState->street;                                                            // 0, 3, 4, or 5 representing when this round ended
        auto myCards = previousState->hands[active];                                                   // your cards
        auto oppCards = previousState->hands[1 - active];                                              // opponent's cards or "" if not revealed

        bool myBountyHit = terminalState->bounty_hits[active];      // true if your bounty hit this round
        bool oppBountyHit = terminalState->bounty_hits[1 - active]; // true if your opponent's bounty hit this round
        int roundNum = gameState->roundNum;

        char bounty_rank = previousState->bounties[active]; // your bounty rank


        int oppStack = previousState->stacks[1 - active];   // the number of chips your opponent has remaining
        int oppContribution = STARTING_STACK - oppStack; // the number of chips your opponent has contributed to the pot
        bool bigBlind = (active == 1);            // true if you are the big blind

        // if we are BB, they should lose 1, else lose 2

        if (bigBlind && oppContribution == 1)
        {
            consecutivePassive++;
        }
        else if (!bigBlind && oppContribution == 2)
        {
            consecutivePassive++;
        }
        else
        {
            consecutivePassive = 0;
        }

        if (consecutivePassive > 30)
        {
            std::cout << "opp is cf bot" << std::endl;
            oppCheckFold = true;
        }
        else
        {
            oppCheckFold = false;
        }

        // // The following is a demonstration of accessing illegal information (will not work)
        // char opponent_bounty_rank = previousState->bounties[1 - active]; // attempting to grab opponent's bounty rank
        // if (myBountyHit)
        // {
        //     std::cout << "I hit my bounty of " << bounty_rank << "!" << std::endl;
        // }
        // if (oppBountyHit)
        // {
        //     std::cout << "Opponent hit their bounty of " << opponent_bounty_rank << "!" << std::endl;
        // }

        totalRounds++;

        if (twoCheckBluff)
        {
            //std::cout << "Two check bluff happened this round" << std::endl;
            pmTwoCheckBluff += myDelta;
            twoCheckBluffCounter++;
        }
        if (threeCheckBluff)
        {
            //std::cout << "Three check bluff happened this round" << std::endl;
            pmThreeCheckBluff += myDelta;
            threeCheckBluffCounter++;
        }
        if (bountyBluff)
        {
            //std::cout << "Bounty bluff happened this round" << std::endl;
            pmBountyBluff += myDelta;
            bountyBluffCounter++;
        }

        if (pmTwoCheckBluff < -300 && twoCheckBluffCounter > 7)
        {
            permanentNoTwoCheck = true;
            std::cout << "Perm no 2c" << std::endl;
        }
        if (pmThreeCheckBluff < -300 && threeCheckBluffCounter > 7)
        {
            permanentNoTwoCheck = true;
            std::cout << "Perm no 3c" << std::endl;
        }
        if (pmBountyBluff < -300 && bountyBluffCounter > 7)
        {
            permanentNoBountyBluff = true;
            std::cout << "Perm no bb" << std::endl;
        }
        
        std::cout << "Opp Bets: " << numOppBets << " | Opp Pot Bets: " << numOppPotBets << " | Opp Bets vs Checks: " << numOppBetNoCheck << " | Opp Checks: " << totalOppChecks << " | Opp Reraises this round: " << oppNumReraise << " | Opp Bets this round: " << oppNumBetsThisRound << std::endl;

        if (numOppBets > 8 && (roundNum % 50 == 0))
        {
            double OppPotBetPercent = numOppPotBets / static_cast<double>(numOppBets);
            
            if (OppPotBetPercent > 0.69) 
            {
                std::cout << "HUGE UNNIT" << std::endl;
                unnitBigBetFact = 2;
            }
            else if (OppPotBetPercent > 0.4)
            {
                std::cout << "UNNIT" << std::endl;
                unnitBigBetFact = 1;
            }
            else
            {
                std::cout << "Opp not betting large often" << std::endl;
                unnitBigBetFact = 0;
            }
        }

        if ((numOppBetNoCheck + totalOppChecks) > 15)
        {
            double OppBetPercent = numOppBetNoCheck / static_cast<double>(numOppBetNoCheck + totalOppChecks);
            if (OppBetPercent > 0.35) //change to 0.44069
            {
                //std::cout << "Opp bluffing A LOT" << std::endl;
                bluffCatcherFact = 1;
            }
            else 
            {
                bluffCatcherFact = 0;
            }
        }

        ourTotalRaises += ourRaisesThisRound;
        oppTotalReraises += oppNumReraise;

        if (ourTotalRaises >= 15 && oppTotalReraises >= 2)
        {
            double oppReraisePct = (double)oppTotalReraises / ourTotalRaises;
            
            if (oppReraisePct > 0.125)
            {
                oppReraiseFact = 1;
                std::cout << "Opp reraise pct: " << oppReraisePct << std::endl;
            }
            else
            {
                oppReraiseFact = 0;
            }
        }

        if (totalRounds == numRounds + 1)
        {
            std::cout << "\n" << std::endl;
            std::cout << "two check bluff: " << pmTwoCheckBluff << std::endl;
            std::cout << "three check bluff: " << pmThreeCheckBluff << std::endl;
            std::cout << "bounty bluff: " << pmBountyBluff << std::endl;
        }

        std::cout << "oppRaiseAsDealer: " << oppRaiseAsDealer << " || oppReraiseAsBB: " << oppReraiseAsBB << " || ourRaiseAsDealer: " << ourRaiseAsDealer << std::endl;

        if ((((float) oppRaiseAsDealer / (float) roundNum) < 0.15) || roundNum < 80)
        {
            oppRaiseAsDealerLess = true;
            std::cout << "ORADL = t" << std::endl;
        }
        else
        {
            oppRaiseAsDealerLess = false;
        }

        if (((float) oppReraiseAsBB / (float) ourRaiseAsDealer > 0.13069 && ourRaiseAsDealer > 15) || roundNum < 80)
        {
            oppReRaiseAsBBMore = true;
            std::cout << "ORRBBM = t" << std::endl;
        }
        else
        {
            oppReRaiseAsBBMore = false;
        }
    }

    int get_rank_index(char rank)
    {
        switch (rank)
        {
        case 'A':
            return 0;
        case 'K':
            return 1;
        case 'Q':
            return 2;
        case 'J':
            return 3;
        case 'T':
            return 4;
        case '9':
            return 5;
        case '8':
            return 6;
        case '7':
            return 7;
        case '6':
            return 8;
        case '5':
            return 9;
        case '4':
            return 10;
        case '3':
            return 11;
        case '2':
            return 12;
        default:
            return 13; // Invalid rank
        }
    }

    std::string categorize_cards(const std::vector<std::string> &cards)
    {
        if (cards.size() != 2)
        {
            std::cerr << "Error: categorize_cards requires exactly two cards." << std::endl;
            return "";
        }

        char rank1 = cards[0][0];
        char rank2 = cards[1][0];
        char suit1 = cards[0][1];
        char suit2 = cards[1][1];

        int rank_idx1 = get_rank_index(rank1);
        int rank_idx2 = get_rank_index(rank2);

        if (rank_idx1 == 13 || rank_idx2 == 13)
        {
            std::cerr << "Error: Invalid card rank encountered." << std::endl;
            return "";
        }

        std::string hpair;
        if (rank_idx1 < rank_idx2)
        {
            hpair = std::string(1, rank1) + std::string(1, rank2);
        }
        else
        {
            hpair = std::string(1, rank2) + std::string(1, rank1);
        }
        char onsuit = (suit1 == suit2) ? 's' : 'o';

        return hpair + onsuit;
    }

    int noIllegalRaises(int myBet, RoundStatePtr roundState, bool active)
    {
        int myPip = roundState->pips[active];      // the number of chips you have contributed to the pot this round of betting
        int oppPip = roundState->pips[1 - active]; // the number of chips your opponent has contributed to the pot this round of betting

        std::array<int, 2> raiseBounds = roundState->raiseBounds();
        int min_raise = raiseBounds[0];
        int max_raise = raiseBounds[1];

        if (myBet < min_raise)
            myBet = min_raise;
        if (myBet > max_raise)
            myBet = max_raise;
        return myBet;
    }

    Action getPreflopAction(RoundStatePtr roundState, int active)
    {
        auto legalActions =
            roundState->legalActions();  // the actions you are allowed to take
        int street = roundState->street; // 0, 3, 4, or 5 representing pre-flop, flop, turn, or river respectively
        // auto myCards = roundState->hands[active];        // your cards
        int oppPip = roundState->pips[1 - active];       // the number of chips your opponent has contributed to the pot this round of betting
        int myPip = roundState->pips[active];
        int continueCost = oppPip - myPip;               // the number of chips needed to stay in the pot
        int myStack = roundState->stacks[active];        // the number of chips you have remaining
        int oppStack = roundState->stacks[1 - active];   // the number of chips your opponent has remaining
        int myContribution = STARTING_STACK - myStack;   // the number of chips you have contributed to the pot
        int oppContribution = STARTING_STACK - oppStack; // the number of chips your opponent has contributed to the pot
        char myBounty = roundState->bounties[active];    // your current bounty rank
        bool bigBlind = (active == 1);                   // true if you are the big blind

        int pot = myContribution + oppContribution;
        int myBet = 0;

        std::vector<std::string> myCards(roundState->hands[active].begin(), roundState->hands[active].end());
        std::string newCards = categorize_cards(myCards);

        int handStrength = -1;

        if (!aggressiveMode)
        {
            handStrength = regularPreflopDict.find(newCards)->second;
        }
        else
        {
            handStrength = aggPreflopDict.find(newCards)->second;
        }

        int oldHandStrength = handStrength;

        if (newCards.find(myBounty) != std::string::npos)
        {
            std::cout << "Bounty ACTIVE with " << myBounty << std::endl;
            hasBounty = true;
            handStrength = 1;
        }



        std::cout << "Hand strength: " << handStrength << std::endl;

        if (!bigBlind && timesBetPreflop == 0) //dealer, first to act
        {
            if (oppCheckFold)
            {
                if (hasBounty)
                {
                    timesBetPreflop++;
                    std::cout << "min raise for cf" << std::endl;
                    return {Action::Type::RAISE, noIllegalRaises(3, roundState, active)};
                }
                else
                {
                    timesBetPreflop++;
                    std::cout << "call for cf" << std::endl;
                    return {Action::Type::CALL};
                }
            }

            if (hasBounty)
            {
                if (aggressiveMode)
                {
                    std::cout << "Agg bad bounty 7x raise from sb" << std::endl;
                    timesBetPreflop++;
                    ourRaiseAsDealer++;
                    myBet = 7 * pot;
                    return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                }
                else if (bluffCatcherFact == 0 || oldHandStrength < 88)
                {
                    std::cout << "3x raise from sb with bounty" << std::endl;
                    timesBetPreflop++;
                    ourRaiseAsDealer++;
                    myBet = 3 * pot;
                    return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                }
                else
                {
                    timesBetPreflop++;
                    std::cout << "Call from sb with bad bounty" << std::endl;
                    return {Action::Type::CALL};

                    // MAYBE CALL ALWAYS HERE TODO!!!!
                }
            }
            else
            {
                if (aggressiveMode)
                {
                    if (handStrength < 26)
                    {
                        std::cout << "Agg 7x raise from sb" << std::endl;
                        timesBetPreflop++;
                        ourRaiseAsDealer++;
                        myBet = 7 * pot;
                        return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                    }
                    else if (handStrength < 88)
                    {
                        std::cout << "Agg 4x raise from sb" << std::endl;
                        timesBetPreflop++;
                        ourRaiseAsDealer++;
                        myBet = 3 * pot;
                        return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                    }
                    else
                    {
                        timesBetPreflop++;
                        std::cout << "Call from sb with bad bounty" << std::endl;
                        return {Action::Type::CALL};
                    }
                }
                else if (handStrength < 26)
                {
                    std::cout << "3x raise from sb" << std::endl;
                    timesBetPreflop++;
                    ourRaiseAsDealer++;
                    myBet = 3 * pot;
                    return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                }
                if (bluffCatcherFact == 1)
                {
                    if ((handStrength < 58 && !oppReRaiseAsBBMore) || (handStrength < 48 && oppReRaiseAsBBMore))
                    {
                        std::cout << "2x raise from sb" << std::endl;
                        timesBetPreflop++;
                        ourRaiseAsDealer++;
                        myBet = 2 * pot;
                        return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                    }
                    else
                    {
                        //std::cout << "Fold from sb" << std::endl;
                        return {Action::Type::FOLD};
                    }
                }
                else if ((handStrength < 88 && !oppReRaiseAsBBMore) || (handStrength < 60 && oppReRaiseAsBBMore))
                {
                    std::cout << "2x raise from sb" << std::endl;
                    timesBetPreflop++;
                    ourRaiseAsDealer++;
                    myBet = 2 * pot;
                    return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                }
                else
                {
                    //std::cout << "Fold from sb" << std::endl;
                    return {Action::Type::FOLD};
                }
            }
        }
        else if (bigBlind && timesBetPreflop == 0) //big blind, haven't acted yet
        {
            if (aggressiveMode)
            {
                if (hasBounty)
                {
                    if (oppPip == 2)
                    {
                        timesBetPreflop++;
                        myBet = 7 * pot;
                        std::cout << "Agg 7x raise from bb from call with bounty" << std::endl;
                        return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                    }
                    else if (oldHandStrength < 26)
                    {
                        timesBetPreflop++;
                        std::cout << "Agg all in from bb with bounty" << std::endl;
                        return {Action::Type::RAISE, noIllegalRaises(400, roundState, active)};
                    }
                    else if (oldHandStrength < 56)
                    {   
                        timesBetPreflop++;
                        myBet = 4 * pot;
                        std::cout << "Agg 7x raise from bb with bounty" << std::endl;
                        return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                    }
                    else if (oppPip < 50)
                    {   
                        std::cout << "Agg call from bb with bounty" << std::endl;
                        timesBetPreflop++;
                        return {Action::Type::CALL};
                    }
                    else
                    {
                        return {Action::Type::FOLD};
                    }
                }
                else
                {
                    if (oppPip == 2 && handStrength < 60)
                    {
                        timesBetPreflop++;
                        myBet = 4 * pot;
                        std::cout << "Agg 7x raise from bb from call" << std::endl;
                        return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                    }
                    else if (handStrength < 15)
                    {
                        if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                        {
                            timesBetPreflop++;
                            std::cout << "Agg all in from bb" << std::endl;
                            return {Action::Type::RAISE, noIllegalRaises(400, roundState, active)};
                        }
                        else
                        {
                            timesBetPreflop++;
                            std::cout << "Agg call from bb" << std::endl;
                            return {Action::Type::CALL};
                        }
                    }
                    else if (handStrength < 28)
                    {   
                        if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                        {
                            timesBetPreflop++;
                            myBet = 7 * pot;
                            std::cout << "Agg 7x raise from bb" << std::endl;
                            return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                        }
                        else
                        {
                            timesBetPreflop++;
                            std::cout << "Agg call from bb" << std::endl;
                            return {Action::Type::CALL};
                        }
                    }
                    else if (oppPip < 50 && handStrength < 88)
                    {   
                        timesBetPreflop++;
                        std::cout << "Agg call from bb" << std::endl;
                        return {Action::Type::CALL};
                    }
                    else if (legalActions.find(Action::Type::CHECK) != legalActions.end())
                    {
                        std::cout << "Opp calls as dealer, check after failed 3x raise as bb" << std::endl;
                        return {Action::Type::CHECK};
                    }
                    else
                    {
                        return {Action::Type::FOLD};
                    }
                }

            }

            if (oppPip == 2) //Opponent calls as dealer
            {
                if (handStrength <= 69 && (legalActions.find(Action::Type::RAISE) != legalActions.end())) //raise with strong hands or bounty
                {
                    timesBetPreflop++;
                    myBet = 3 * pot;
                    std::cout << "Opp calls as dealer, 3x raise from bb" << std::endl;
                    return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                }
                else
                {
                    std::cout << "Opp calls as dealer, check after failed 3x raise as bb" << std::endl;
                    return {Action::Type::CHECK};
                }
            }

            oppRaiseAsDealer++;

            if (((((handStrength < 9 || (handStrength <= 61 && oppPip <= 5) || (handStrength <= 46 && oppPip <= 12) || (handStrength <= 12 && oppPip <= 25)) && !oppRaiseAsDealerLess) ||
                ((handStrength < 9 || (handStrength <= 41 && oppPip <= 5) || (handStrength <= 32 && oppPip <= 12) || (handStrength <= 9 && oppPip <= 25)) && oppRaiseAsDealerLess)) && bluffCatcherFact == 0) ||
                ((((handStrength < 9 || (handStrength <= 46 && oppPip <= 5) || (handStrength <= 31 && oppPip <= 12) || (handStrength <= 9 && oppPip <= 25)) && !oppRaiseAsDealerLess) ||
                ((handStrength < 9 || (handStrength <= 32 && oppPip <= 5) || (handStrength <= 25 && oppPip <= 12) || (handStrength <= 9 && oppPip <= 25)) && oppRaiseAsDealerLess)) && bluffCatcherFact == 1)
            )  //Always get here w bounty
            {
                timesBetPreflop++;
                myBet = 3 * pot;

                if (oldHandStrength >= 9 && hasBounty) //weak hands with bounty
                {
                    if (((((oppPip <= 6) || (oldHandStrength <= 180 && oppPip <= 12) || (oldHandStrength <= 18 && oppPip <= 30) && !oppRaiseAsDealerLess) ||
                        ((oldHandStrength <= 88 && oppPip <= 12) || (oldHandStrength <= 12 && oppPip <= 30) && oppRaiseAsDealerLess)) && bluffCatcherFact == 0) ||
                        ((((oppPip <= 6) || (oldHandStrength <= 61 && oppPip <= 12) || (oldHandStrength <= 14 && oppPip <= 30) && !oppRaiseAsDealerLess) ||
                        ((oldHandStrength <= 41 && oppPip <= 12) || (oldHandStrength <= 12 && oppPip <= 30) && oppRaiseAsDealerLess)) && bluffCatcherFact == 1)
                    ) //when to reraise with bounty
                    {
                        if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                        {
                            std::cout << "3x raise from bb with bounty" << std::endl;
                            return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                        }
                        else if (legalActions.find(Action::Type::CALL) != legalActions.end())
                        {
                            std::cout << "Call after failed 3x raise from bb with bounty" << std::endl;
                            return {Action::Type::CALL};
                        }
                        else
                        {
                            std::cout << "Error: No legal actions found with bounty" << std::endl;
                        }
                    }
                    else if (oppPip > 150)
                    {
                        if (oldHandStrength <= 10) //calling all in as bb with bounty
                        {
                            if (legalActions.find(Action::Type::CALL) != legalActions.end())
                            {
                                std::cout << "Call huge bet from oppoent as dealer from bb with bounty" << std::endl;
                                return {Action::Type::CALL};
                            }
                            else
                            {
                                std::cout << "Error: No legal actions found with bounty" << std::endl;
                            }
                        }
                        else
                        {
                            std::cout << "Bounty hand fold as bb to large bet" << std::endl;
                            return {Action::Type::FOLD};
                        }
                    }
                    else if (oppPip <= 25 && oldHandStrength <= 120) //fold super shitters with bounty else call, great pot odds. Note rarely get here - only if opponent raises between 13-20.
                    {
                        if (legalActions.find(Action::Type::CALL) != legalActions.end())
                        {
                            std::cout << "Call from bb otherwise with bounty" << std::endl;
                            return {Action::Type::CALL};
                        }
                        else
                        {
                            std::cout << "Check after failed call from bb otherwise with bounty" << std::endl;
                            return {Action::Type::CHECK};
                        }
                    }
                    else if (oppPip > 25 && oppPip <= 150 && oldHandStrength < (54 - pow((oppPip - 2) / 398.0, 1.0 / 3.0) * (61))) // when to call with bounty in bb otherwise
                    {
                        if (legalActions.find(Action::Type::CALL) != legalActions.end())
                        {
                            std::cout << "Call from bb otherwise with bounty" << std::endl;
                            return {Action::Type::CALL};
                        }
                        else
                        {
                            std::cout << "Check after failed call from bb otherwise with bounty" << std::endl;
                            return {Action::Type::CHECK};
                        }
                    }
                    else 
                    {
                        //std::cout << "Bounty hand fold as bb" << std::endl;
                        return {Action::Type::FOLD};
                    }
                }
               
                if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                {
                    std::cout << "3x raise from bb" << std::endl;
                    return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                }
                else if (legalActions.find(Action::Type::CALL) != legalActions.end())
                {
                    std::cout << "Call after failed 3x raise from bb" << std::endl;
                    return {Action::Type::CALL};
                }
                else
                {
                    std::cout << "Error: No legal actions found" << std::endl;
                }
            }
            else if (oppPip > 150)
            {
                if (oldHandStrength <= 8) //calling all in as bb without bounty
                {
                    if (legalActions.find(Action::Type::CALL) != legalActions.end())
                    {
                        std::cout << "Call huge bet from oppoent as dealer from bb" << std::endl;
                        return {Action::Type::CALL};
                    }
                    else
                    {
                        std::cout << "Error: No legal actions found" << std::endl;
                    }
                }
                else
                {
                    std::cout << "fold as bb to large bet" << std::endl;
                    return {Action::Type::FOLD};
                }
            }
            else if (handStrength < (85 + 1 - pow((oppPip - 2) / 198.0, 1.0 / 3.0) * (88 + 1 - 5)) && oppPip <= 150) //same as before
            {
                if (legalActions.find(Action::Type::CALL) != legalActions.end())
                {
                    std::cout << "Call from bb otherwise" << std::endl;
                    return {Action::Type::CALL};
                }
                else
                {
                    std::cout << "Error: Check after failed call from bb otherwise" << std::endl;
                    return {Action::Type::CHECK};
                }
            }
            else
            {
                if (legalActions.find(Action::Type::CHECK) != legalActions.end())
                {
                    std::cout << "Error: Failed Check from bb otherwise" << std::endl;
                    return {Action::Type::CHECK};
                }
                else
                {
                    //std::cout << "Fold from bb" << std::endl;
                    return {Action::Type::FOLD};
                }
            }
        }
        else if (aggressiveMode)
        {
            if (hasBounty)
            {
                if (oldHandStrength < 26)
                {
                    if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                    {
                        timesBetPreflop++;
                        std::cout << "Agg all in with bounty" << std::endl;
                        return {Action::Type::RAISE, noIllegalRaises(400, roundState, active)};
                    }
                    else
                    {
                        timesBetPreflop++;
                        std::cout << "Agg call from bb" << std::endl;
                        return {Action::Type::CALL};
                    }    
                }
                else if (continueCost < 50)
                {
                    std::cout << "Agg call with bounty" << std::endl;
                    return {Action::Type::CALL};
                }
                else
                {
                    return {Action::Type::FOLD};
                }
            }
            else
            {
                if (handStrength < 15)
                {
                    if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                    {
                        timesBetPreflop++;
                        std::cout << "Agg all in without bounty" << std::endl;
                        return {Action::Type::RAISE, noIllegalRaises(400, roundState, active)};
                    }
                    else
                    {
                        timesBetPreflop++;
                        std::cout << "Agg call from bb" << std::endl;
                        return {Action::Type::CALL};
                    }  
                }
                else if (continueCost < 50 && handStrength < 40)
                {
                    std::cout << "Agg call" << std::endl;
                    return {Action::Type::CALL};
                }
                else
                {
                    return {Action::Type::FOLD};
                }
            }    
        }
        else if (!bigBlind && timesBetPreflop == 1) //We are dealer, raise, get reraised from bb
        {
            oppReraiseAsBB++;
            if (hasBounty) 
            {
                if (((oldHandStrength <= 8 || (oldHandStrength <= 12 && oppPip <= 50)) && !oppReRaiseAsBBMore) ||
                    ((oldHandStrength <= 16 || (oldHandStrength <= 19 && oppPip <= 50)) && oppReRaiseAsBBMore) 
                ) // reraise w bounty - can change if if we notice opp reraising as bb often.
                {
                    timesBetPreflop++;
                    myBet = 2 * pot;
                    if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                    {
                        std::cout << "2x reraise from raise with bounty" << std::endl;
                        return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                    }
                    else if (legalActions.find(Action::Type::CALL) != legalActions.end())
                    {
                        std::cout << "Call after failed 2x reraise from raise with bounty" << std::endl;
                        return {Action::Type::CALL};
                    }
                    else
                    {
                        std::cout << "Error: No legal actions found" << std::endl;
                    }
                }
                else if (oppPip >= 150) 
                {
                    if (oldHandStrength <= 10) //TODO - dealing with all in bots
                    {
                        if (legalActions.find(Action::Type::CALL) != legalActions.end())
                        {
                            std::cout << "Call huge raise from bb with bounty" << std::endl;
                            return {Action::Type::CALL};
                        }
                        else
                        {
                            std::cout << "Error: Check after failed call from reraise otherwise" << std::endl;
                            return {Action::Type::CHECK};
                        }
                    }
                    else
                    {
                        std::cout << "Fold to huge bet from bb with bounty" << std::endl;
                        return {Action::Type::FOLD};
                    }
                }
                else if (oppPip > 40 && oppPip <= 150 && (oldHandStrength < (92 - pow((oppPip - 2) / 198.0, 1.0 / 3.0) * 90))) //call with bounty as dealer from bb raise
                {
                    if (legalActions.find(Action::Type::CALL) != legalActions.end())
                    {
                        std::cout << "Call from large reraise from bb with bounty" << std::endl;
                        return {Action::Type::CALL};
                    }
                    else
                    {
                        std::cout << "Error: Check after failed call from reraise otherwise" << std::endl;
                        return {Action::Type::CHECK};
                    }
                }
                else if (oppPip <= 40 && oldHandStrength <= 87) //can call with most hands to 4-bets with bounty. In og preflop this was 110, chanmged to 88 to cut out some shitters
                {
                    if (legalActions.find(Action::Type::CALL) != legalActions.end())
                    {
                        std::cout << "Call from small reraise from bb with bounty" << std::endl;
                        return {Action::Type::CALL};
                    }
                    else
                    {
                        std::cout << "Error: Check after failed call from reraise otherwise" << std::endl;
                        return {Action::Type::CHECK};
                    }
                }
                else
                {
                    std::cout << "Fold to reraise from bb with bounty" << std::endl;
                    return {Action::Type::FOLD};
                }

            }

            if (((handStrength <= 8) && !oppReRaiseAsBBMore) ||
                ((handStrength <= 9 || (handStrength <= 15 && oppPip <= 50)) && oppReRaiseAsBBMore)
            ) //reraise without bounty or call big blind raises for more than 150
            {
                timesBetPreflop++;
                myBet = 2 * pot;

                if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                {
                    std::cout << "2x reraise from raise" << std::endl;
                    return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                }
                else if (legalActions.find(Action::Type::CALL) != legalActions.end())
                {
                    std::cout << "Call after failed 2x reraise from raise" << std::endl;
                    return {Action::Type::CALL};
                }
                else
                {
                    std::cout << "Error: No legal actions found" << std::endl;
                }
            }
            else if (oppPip <= 150 && handStrength <= (90 - pow((oppPip - 2) / 198.0, 1.0 / 3.0) * 90)) //call without bounty. TODO - change all in calling if opp is all-in bot
            {
                if (legalActions.find(Action::Type::CALL) != legalActions.end())
                {
                    std::cout << "Call from reraise otherwise" << std::endl;
                    return {Action::Type::CALL};
                }
                else
                {
                    std::cout << "Error: Check after failed call from reraise otherwise" << std::endl;
                    return {Action::Type::CHECK};
                }
            }
            else //fold without bounty
            {
                std::cout << "Fold to reraise from bb" << std::endl;
                return {Action::Type::FOLD};
            }
        }
        else  //any other situation after each team has acted once preflop
        {
            if (hasBounty) 
            {
                if (oldHandStrength <= 5) // reraise with bounty
                {
                    timesBetPreflop++;
                    myBet = 2 * pot;
                    if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                    {
                        std::cout << "2x reraise from raise with bounty" << std::endl;
                        return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                    }
                    else if (legalActions.find(Action::Type::CALL) != legalActions.end())
                    {
                        std::cout << "Call after failed 2x reraise from raise with bounty" << std::endl;
                        return {Action::Type::CALL};
                    }
                    else
                    {
                        std::cout << "Error: No legal actions found" << std::endl;
                    }
                }
                else if (continueCost <= 40 && oldHandStrength <= 87) //before we always call with top 110 here with bounty for <40, decreased a bit so we aren't calling behind as often. But potted in a lot
                {
                    if (legalActions.find(Action::Type::CALL) != legalActions.end())
                        {
                            std::cout << "WEEEWOOOWEEEWOOO Call huge raise with bounty" << std::endl;
                            alarmBell = true;
                            return {Action::Type::CALL};
                        }
                        else
                        {
                            std::cout << "Error: Check after failed call from reraise otherwise" << std::endl;
                            return {Action::Type::CHECK};
                        }
                }
                else if (oldHandStrength <= (71 - pow((oppPip - 2) / 398.0, 1.0 / 3.0) * 61)) //call with bounty
                {
                    if (legalActions.find(Action::Type::CALL) != legalActions.end())
                    {
                        std::cout << "WEEEWOOOWEEEWOOO Call from large reraise with bounty" << std::endl;
                        alarmBell = true;
                        return {Action::Type::CALL};
                    }
                    else
                    {
                        std::cout << "Error: Check after failed call from reraise otherwise" << std::endl;
                        return {Action::Type::CHECK};
                    }
                }
                else
                {
                    std::cout << "Fold to reraise with bounty" << std::endl;
                    return {Action::Type::FOLD};
                }

            }

            if (handStrength <= 5) //reraise without bounty
            {
                timesBetPreflop++;
                myBet = 2 * pot;

                if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                {
                    std::cout << "2x reraise from raise" << std::endl;
                    return {Action::Type::RAISE, noIllegalRaises(myBet, roundState, active)};
                }
                else if (legalActions.find(Action::Type::CALL) != legalActions.end())
                {
                    std::cout << "Call after failed 2x reraise from raise" << std::endl;
                    return {Action::Type::CALL};
                }
                else
                {
                    std::cout << "Error:  legal actions found" << std::endl;
                }
            }
            else if (handStrength <= (67 - pow((oppPip - 2) / 398.0, 1.0 / 3.0) * 61)) //call without bounty // TODO calling all ins?
            {
                if (legalActions.find(Action::Type::CALL) != legalActions.end())
                {
                    std::cout << "WEEEWOOOWEEEWOOO Call from reraise otherwise" << std::endl;
                    alarmBell = true;
                    return {Action::Type::CALL};
                }
                else
                {
                    std::cout << "Error: Check after failed call from reraise otherwise" << std::endl;
                    return {Action::Type::CHECK};
                }
            }
            else //fold without bounty
            {
                std::cout << "Fold to reraise from bb" << std::endl;
                return {Action::Type::FOLD};
            }
        }
        std::cout << "Error: SHOULD NEVER BE HERE" << std::endl;
        return {Action::Type::FOLD};
    }

    std::pair<Action, int> getPostflopAction(double handStrength, RoundStatePtr roundState, int active)
    {

        /*
        returns {action, then action type} pair
        */
        auto legalActions = roundState->legalActions();  // the actions you are allowed to take
        int street = roundState->street; // 0, 3, 4, or 5 representing pre-flop, flop, turn, or river respectively
        // auto myCards = roundState->hands[active];        // your cards
        // auto boardCards = roundState->deck;              // the board cards
        int myPip = roundState->pips[active];            // the number of chips you have contributed to the pot this round of betting
        int oppPip = roundState->pips[1 - active];       // the number of chips your opponent has contributed to the pot this round of betting
        int myStack = roundState->stacks[active];        // the number of chips you have remaining
        int oppStack = roundState->stacks[1 - active];   // the number of chips your opponent has remaining
        int continueCost = oppPip - myPip;               // the number of chips needed to stay in the pot
        int myContribution = STARTING_STACK - myStack;   // the number of chips you have contributed to the pot
        int oppContribution = STARTING_STACK - oppStack; // the number of chips your opponent has contributed to the pot
        char myBounty = roundState->bounties[active];    // your current bounty rank
        bool bigBlind = (active == 1);                   // true if you are the big blind

        int pot = myContribution + oppContribution;

        if (!nitToggle)
        {
            handStrength = pow(handStrength,1.2);
            std::cout << "NEW HAND STRENGTH: " << handStrength << std::endl;
        }
        auto board = roundState->deck;

        if (!hasBounty)
        {
            for (int i = 0; i < street; ++i)
            {
                const auto &card = board[i];
                if (card.find(myBounty) != std::string::npos)
                {
                    std::cout << "Bounty is ACTIVE from board with bounty " << myBounty << std::endl;
                    hasBounty = true;
                }
            }
        }

        std::vector<Card> myCards;
        for (const auto &cardStr : roundState->hands[active])
        {
            try
            {
                Card c(generateCardCodeFromString(cardStr));
                myCards.emplace_back(c);
                // std::cout << "My card: ";
                // c.print();
                // std::cout << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error converting my card: " << e.what() << std::endl;
            }
        }

        std::vector<Card> boardCards;
        for (int i = 0; i < street; ++i)
        {
            const auto &cardStr = roundState->deck[i];
            try
            {
                Card c(generateCardCodeFromString(cardStr));
                boardCards.emplace_back(c);
                // std::cout << "Board card: ";
                // c.print();
                // std::cout << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error converting board card: " << e.what() << std::endl;
            }
        }

        double randPercent = (rand() / double(RAND_MAX));

        // if opponent bets
        if (oppPip > 0)
        {
            oppLastContribution = oppContribution;
            numOppChecks = 0;
            numOppBets++;
            oppNumBetsThisRound++;
            if (myPip == 0)
            {
                numOppBetNoCheck++;
            }
            else
            {
                oppNumReraise++;
            }

            std::cout << "Opp bets" << std::endl;
            oppBetLastRound = true;
        }
        else if (!bigBlind && oppPip == 0)
        {
            std::cout << "Opp checks from bb" << std::endl;
            numOppChecks++;
            totalOppChecks++;
        }
        else if (bigBlind && street > 3 && oppContribution == oppLastContribution && !oppBetLastRound)
        {
            std::cout << "Opp checks from prev street" << std::endl;
            numOppChecks++;
            totalOppChecks++;
        }

        if (legalActions.find(Action::Type::CHECK) != legalActions.end()) //Check or Raise
        {
            if (oppCheckFold && ourRaisesThisRound == 0 && oppNumBetsThisRound == 0)
            {
                std::cout << "opp is cf bot" << std::endl;
                if (hasBounty)
                {
                    std::cout << "min raise for cf" << std::endl;
                    return {{Action::Type::RAISE}, 6};
                }
                else if (street == 5)
                {
                    std::cout << "min raise for cf w no bounty" << std::endl;
                    return {{Action::Type::RAISE}, 6};
                }
                else
                {
                    return {{Action::Type::CHECK}, -1};
                }
            }


            oppBetLastRound = false;
            std::cout << "Able to check or out of position" << std::endl;

            if (hasBounty && handStrength < 0.7 && (!permanentNoBountyBluff || (aggressiveMode && randPercent < 0.5)))
            {
                bountyRaises++;

                if (bountyRaises > 1)
                {
                    std::cout << "I stop bounty bluff raising due to failed attempt" << std::endl;
                }
                else if (alarmBell)
                {
                    std::cout << "I stop bounty bluff raising due to alarm bell preflop" << std::endl;
                }
                else if (ourRaisesThisRound > 0)
                {
                    std::cout << "I stop bounty bluff raising due to us already raising this round" << std::endl;
                }
                else if (oppNumBetsThisRound > 1)
                {
                    std::cout << "I stop bounty bluff raising due to opp raising twice this round" << std::endl;
                }
                else if (street == 3 && handStrength > 0.65)
                {
                    std::cout << "Blocker bet with bounty for value: #" << bountyRaises << std::endl;
                    numOppChecks = 0;
                    numSelfChecks = 0;
                    ourRaisesThisRound++;
                    bountyBluff = true;
                    return {{Action::Type::RAISE}, 1};
                }
                else if ((randPercent < 0.60 && nitToggle) || aggressiveMode)
                {
                    std::cout << "I randomly bounty bluff raise #" << bountyRaises << std::endl;
                    numOppChecks = 0;
                    numSelfChecks = 0;
                    ourRaisesThisRound++;
                    bountyBluff = true;
                    return {{Action::Type::RAISE}, 4};
                }
                else
                {
                    std::cout << "Randomly do not bluff raise bounty" << std::endl;
                }
            }

            if (hasBounty && handStrength > 0.75)
            {
                std::cout << "I try to value bounty raise" << std::endl;
            }

            double raiseStrength = 0.7 + ((street % 3) * (double)raiseFactor) + (double)ourRaisesThisRound*0.02 + (double)oppNumBetsThisRound * 0.02 + (double)oppNumReraise * 0.05;
            raiseStrength = std::min(raiseStrength, 0.9);
            std::cout << "Raise Strength: " << raiseStrength << std::endl;

            double checkNutsStrength = 0.815 + 0.03 * (street % 3);
            double randPercent4 = (rand() / double(RAND_MAX));
            if (bigBlind && (bluffCatcherFact == 1 || (bluffCatcherFact == 0 && randPercent4 < 0.4)) && randPercent < 0.75 && handStrength > checkNutsStrength && (street == 3 || (street == 4 && oppNumBetsThisRound > 0 && ourRaisesThisRound < 1)))
            {
                std::cout << "I check deception against agg team with strong hand" << std::endl;
                numSelfChecks++;
                return {{Action::Type::CHECK}, -1};
            }

            else if (((randPercent < handStrength + 0.15 || street == 5)) && (handStrength >= raiseStrength))
            {
                numOppChecks = 0;
                numSelfChecks = 0;
                ourRaisesThisRound++;
                //std::cout << "I random raise for value with handStrength " << handStrength << std::endl;
                return {{Action::Type::RAISE}, 1};
            }
            else if (alarmBell && numOppChecks >= 2)
            {
                std::cout << "I stop two/three check bluff for alarm bell" << std::endl;
                numSelfChecks++;
                return {{Action::Type::CHECK}, -1};
            }
            else if (numOppChecks == 2 && !permanentNoTwoCheck && pot < 250 && nitToggle)
            {
                numOppChecks = 0;
                numSelfChecks = 0;
                ourRaisesThisRound++;
                std::cout << "I raise for 2 check bluff" << std::endl;
                twoCheckBluff = true;
                return {{Action::Type::RAISE}, 2};
            }
            else if (numOppChecks == 3 && !permanentNoThreeCheck && pot < 250)
            {
                numOppChecks = 0;
                numSelfChecks = 0;
                ourRaisesThisRound++;
                std::cout << "I raise for 3 check bluff" << std::endl;
                threeCheckBluff = true;
                return {{Action::Type::RAISE}, 3};
            }

            std::cout << "I check" << std::endl;
            numSelfChecks++;
            return {{Action::Type::CHECK}, -1};
        }
        // opponent raises or reraises
        else
        {
            std::cout << "Opp raises/reraises" << std::endl;

            double realPotOdds = (double)continueCost / (pot - continueCost); //percent of pot needed to call
            if (realPotOdds > 1.09) 
            {
                numOppPotBets++;
            }

            std::cout << "Real pot odds: " << realPotOdds << std::endl;

            // change pot odds

            double changedPotOdds = realPotOdds;

            if (realPotOdds > 1.7)
            {
                changedPotOdds = 0.82 + (street % 3)*0.02;
                changedPotOdds += 0.04 * (double)oppNumReraise;
                if (oppNumBetsThisRound > 2)
                {
                    changedPotOdds += 0.03;
                }
                else if (ourRaisesThisRound >= 2 && myPip == 0 && street == 5)
                {
                    changedPotOdds += 0.015;
                }
                changedPotOdds = std::min(0.86 + ((street % 3) * 0.01), changedPotOdds);
            }
            else if (realPotOdds > 1.1)
            {
                changedPotOdds = 0.77 + (street % 3)*0.02;
                changedPotOdds += 0.06 * (double)oppNumReraise;
                if (oppNumBetsThisRound > 2)
                {
                    changedPotOdds += 0.04;
                }
                else if (ourRaisesThisRound >= 2 && myPip == 0 && street == 5)
                {
                    changedPotOdds += 0.02;
                }
                changedPotOdds = std::min(0.86 + ((street % 3) * 0.01), changedPotOdds);
            }
            else if (realPotOdds > 0.8)
            {
                changedPotOdds = 0.695 + (street % 3)*0.03;
                changedPotOdds += 0.135 * (double)oppNumReraise;
                if (oppNumBetsThisRound > 2)
                {
                    changedPotOdds += 0.11;
                }
                else if (ourRaisesThisRound >= 2 && myPip == 0 && street == 5)
                {
                    changedPotOdds += 0.06;
                }
                changedPotOdds = std::min(0.85 + ((street % 3) * 0.01), changedPotOdds);
            }
            else if (realPotOdds > 0.7)
            {
                changedPotOdds = 0.645 + (street % 3)*0.03;
                changedPotOdds += 0.18 * (double)oppNumReraise;
                if (oppNumBetsThisRound > 2)
                {
                    changedPotOdds += 0.14;
                }
                else if (ourRaisesThisRound >= 2 && myPip == 0 && street == 5)
                {
                    changedPotOdds += 0.07;
                }
                changedPotOdds = std::min(0.83 + ((street % 3) * 0.02), changedPotOdds);
            }
            else
            {
                changedPotOdds = std::min(realPotOdds + 0.075, 0.645);
                changedPotOdds += 0.19 * (double)oppNumReraise;
                if (oppNumBetsThisRound > 2)
                {
                    changedPotOdds += 0.19;
                }
                else if (ourRaisesThisRound >= 2 && myPip == 0 && street == 5)
                {
                    changedPotOdds += 0.11;
                }
                changedPotOdds = std::min(0.82 + ((street % 3) * 0.02), changedPotOdds);
            }

            if (realPotOdds < 0.5)
            {
                changedPotOdds = std::min(realPotOdds + 0.125, 0.575);
                changedPotOdds += 0.3 * (double)oppNumReraise;
                if (oppNumBetsThisRound > 2)
                {
                    changedPotOdds += 0.3;
                }
                else if (ourRaisesThisRound >= 2 && myPip == 0 && street == 5)
                {
                    changedPotOdds += 0.175;
                }
                changedPotOdds = std::min(0.815 + ((street % 3) * 0.02), changedPotOdds);
            }

            else if (realPotOdds >= 1.1) 
            {
                if (oppNumReraise == 0 && oppNumBetsThisRound < 3)
                {
                    changedPotOdds -= 0.06 * (double)unnitBigBetFact;
                }
                else
                {
                    changedPotOdds -= 0.03 * (double)unnitBigBetFact;
                }
                
            }

            if (myPip == 0 && oppNumReraise == 0 && realPotOdds > 0.425 && realPotOdds < 1.4)
            {
                changedPotOdds -= (double)bluffCatcherFact * 0.1;
            }
            else if (myPip > 0 && realPotOdds > 0.375)
            {
                changedPotOdds -= (double)oppReraiseFact * 0.075;
            }

            if (hasBounty && realPotOdds < 1.1 && oppNumReraise < 1 && oppNumBetsThisRound < 3)
            {
                changedPotOdds -= 0.05; //slight unnit with bounty to smaller bets (not reraises or continued betting from opponent)
            }

            if (aggressiveMode)
            {
                changedPotOdds -= 0.075;
            }

            std::cout << "Changed pot odds: " << changedPotOdds << std::endl;

            if (handStrength < changedPotOdds) // TODO FIX CALLING LOOSE/NIT
            {
                // TODO MAYBE ADD FLOATING ON THE FLOP?
                return {{Action::Type::FOLD}, -1};
            }
            else if (changedPotOdds >= 0.2 && (handStrength < 0.35 + (street%3)*.1)) //only auto fold shitters to decent sized bets
            {
                return {{Action::Type::FOLD}, -1};
            }
            else
            {
                double reraiseStrength = (0.86 + ((street % 3) * (double)reRaiseFactor));
                reraiseStrength += oppNumReraise * 0.04; // increase reraise strength if opponent reraises
                
                if (realPotOdds > 1.1) //more nitty reraising against huge opponent bets
                {
                    reraiseStrength += 0.01 * (2 - unnitBigBetFact);
                }
                if (reraiseStrength > 0.94) // more nitty cap against reraising
                {
                    reraiseStrength = 0.94;
                }
                double randPercent3 = (rand() / double(RAND_MAX));

                if (handStrength >= reraiseStrength || (handStrength - changedPotOdds > 0.5 && handStrength >= reraiseStrength - 0.05))
                {
                    if (street == 3 || (street == 4 && randPercent3 < 0.75 && bluffCatcherFact == 1))
                    {
                        double randPercent2 = (rand() / double(RAND_MAX));
                        if (pot < 100 && (randPercent2 < 0.5 || (randPercent2 < 0.85 && bluffCatcherFact == 1)))
                        {
                            std::cout << "I call with nuts deception" << std::endl;
                            return {{Action::Type::CALL}, -1};
                        }
                        else
                        {
                            std::cout << "I min-click reraise for max value" << std::endl;
                            numOppChecks = 0;
                            numSelfChecks = 0;
                            ourRaisesThisRound++;
                            return {{Action::Type::RAISE}, 6};
                        }
                    }
                    else
                    {
                        std::cout << "I reraise" << std::endl;
                        numOppChecks = 0;
                        numSelfChecks = 0;
                        ourRaisesThisRound++;
                        return {{Action::Type::RAISE}, 5};
                    }
                }
            }
            std::cout << "I call" << std::endl;
            return {{Action::Type::CALL}, -1};
        }
    }

    int getPostflopBetSize(double handStrength, RoundStatePtr roundState, int active, int actionCategory)
    {
        // 1 is value, 2 is 2 check bluff, 3 is 3 check bluff

        auto legalActions =
            roundState->legalActions();  // the actions you are allowed to take
        int street = roundState->street; // 0, 3, 4, or 5 representing pre-flop, flop, turn, or river respectively
        // auto myCards = roundState->hands[active];        // your cards
        int oppPip = roundState->pips[1 - active];       // the number of chips your opponent has contributed to the pot this round of betting
        int myStack = roundState->stacks[active];        // the number of chips you have remaining
        int oppStack = roundState->stacks[1 - active];   // the number of chips your opponent has remaining
        int myContribution = STARTING_STACK - myStack;   // the number of chips you have contributed to the pot
        int oppContribution = STARTING_STACK - oppStack; // the number of chips your opponent has contributed to the pot
        char myBounty = roundState->bounties[active];    // your current bounty rank
        bool bigBlind = (active == 1);                   // true if you are the big blind

        int pot = myContribution + oppContribution;

        double randPercent = (rand() / double(RAND_MAX));

        double nutsThreshold = 0.87 + 0.02 * (street % 3); //nutted
        double secondThreshold = 0.80 + 0.03 * (street % 3);


        // bluffing raise
        if (actionCategory == 4 || actionCategory == 3 || actionCategory == 2)
        {
            return noIllegalRaises(int((std::max(randPercent + 0.55, 1.1)) * pot), roundState, active); //1.1 - 1.55x pot for bluff
        }
        // value raise
        else if (actionCategory == 5) //reraises
        {
            return noIllegalRaises(int((std::max(randPercent + 0.7, 1.2)) * pot), roundState, active); //reraises
        }
        else if (actionCategory == 6) //min click reraise
        {
            return noIllegalRaises(1, roundState, active); //reraises
        }
        else if (actionCategory == 1 && handStrength >= nutsThreshold)
        {
            if (pot >= 20 && street != 5)
            {
                return noIllegalRaises(int((std::max(randPercent - 0.1, 0.5)) * pot), roundState, active); //try potting opponent in with the nuts early in hand
            }
            else
            {
                return noIllegalRaises(int((std::max(randPercent + 0.85, 1.2)) * pot), roundState, active); // 1.2-1.85x pot
            }
        }
        
        else
        {
            if (handStrength > secondThreshold)
            {
                std::cout << "randPercent" << std::endl;
                return noIllegalRaises(int((randPercent + 0.5) * pot), roundState, active); //0.5-1.5x pot for value
            }
            else
            {
                std::cout << "randPercent" << std::endl;  //TODO - blocker bets?? change sizing on slightly weaker value hands to block 
                return noIllegalRaises(int((randPercent * 0.5 + 0.4) * pot), roundState, active); //0.4-0.9x pot for value
            }
        }
    }

    Action getAction(GameInfoPtr gameState, RoundStatePtr roundState, int active)
    {
        auto legalActions =
            roundState->legalActions();  // the actions you are allowed to take
        int street = roundState->street; // 0, 3, 4, or 5 representing pre-flop, flop, turn, or river respectively
        // auto myCards = roundState->hands[active];        // your cards
        // auto boardCards = roundState->deck;              // the board cards
        int myPip = roundState->pips[active];            // the number of chips you have contributed to the pot this round of betting
        int oppPip = roundState->pips[1 - active];       // the number of chips your opponent has contributed to the pot this round of betting
        int myStack = roundState->stacks[active];        // the number of chips you have remaining
        int oppStack = roundState->stacks[1 - active];   // the number of chips your opponent has remaining
        int continueCost = oppPip - myPip;               // the number of chips needed to stay in the pot
        int myContribution = STARTING_STACK - myStack;   // the number of chips you have contributed to the pot
        int oppContribution = STARTING_STACK - oppStack; // the number of chips your opponent has contributed to the pot
        char myBounty = roundState->bounties[active];    // your current bounty rank

        double handStrength;

        std::pair<Action, int> postflopAction;

        if (alreadyWon)
        {
            return {Action::Type::FOLD};
        }
        if (street == 0)
        {
            return getPreflopAction(roundState, active);
        }
        else
        {

            if (street == 3)
            {
                oppLastContribution = oppContribution;
            }

            std::vector<Card> allCards;

            int winCount = 0;

            std::vector<Card> myCards;
            for (const auto &cardStr : roundState->hands[active])
            {
                try
                {
                    Card c(generateCardCodeFromString(cardStr));
                    myCards.emplace_back(c);
                    // std::cout << "My card: ";
                    // c.print();
                    // std::cout << std::endl;
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error converting my card: " << e.what() << std::endl;
                }
            }

            std::vector<Card> boardCards;
            for (int i = 0; i < street; ++i)
            {
                const auto &cardStr = roundState->deck[i];
                try
                {
                    Card c(generateCardCodeFromString(cardStr));
                    boardCards.emplace_back(c);
                    // std::cout << "Board card: ";
                    // c.print();
                    // std::cout << std::endl;
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error converting board card: " << e.what() << std::endl;
                }
            }

            std::vector<Card> knownCards = myCards;
            knownCards.insert(knownCards.end(), boardCards.begin(), boardCards.end());

            std::vector<Card> remainingDeck;
            for (size_t i = 0; i < deckInstance.size(); ++i)
            {
                const Card &card = deckInstance[i];
                bool isKnown = false;
                for (const auto &known : knownCards)
                {
                    if (card.code == known.code)
                    {
                        isKnown = true;
                        break;
                    }
                }
                if (!isKnown)
                {
                    remainingDeck.emplace_back(card);
                }
            }

            std::mt19937 rng(std::random_device{}());

            int cardsToAdd = 5 - street;

            for (int trial = 0; trial < numMCTrials; ++trial)
            {
                std::shuffle(remainingDeck.begin(), remainingDeck.end(), rng);

                std::vector<Card> oppCards = {remainingDeck[0], remainingDeck[1]};

                std::vector<Card> trialBoard = boardCards;
                for (int i = 0; i < cardsToAdd; ++i)
                {
                    trialBoard.emplace_back(remainingDeck[2 + i]);
                }

                std::vector<Card> playerAllCards = myCards;
                playerAllCards.insert(playerAllCards.end(), trialBoard.begin(), trialBoard.end());

                std::vector<Card> oppAllCards = oppCards;
                oppAllCards.insert(oppAllCards.end(), trialBoard.begin(), trialBoard.end());

                BestHandResult playerBest = evalHand(playerAllCards);
                BestHandResult oppBest = evalHand(oppAllCards);

                if (playerBest.minVal < oppBest.minVal)
                {
                    winCount += 2;
                }
                else if (playerBest.minVal == oppBest.minVal)
                {
                    winCount++;
                }
            }

            int divisor = 2*numMCTrials;
            handStrength = (static_cast<double>(winCount) / static_cast<double>(divisor));
            std::cout << "MC Simulation: " << handStrength << " for street " << street << std::endl;

            postflopAction = getPostflopAction(handStrength, roundState, active);
        }

        auto actionPostflop = postflopAction.first;
        auto actionCategory = postflopAction.second;

        if (actionCategory == -1)
        {
            return actionPostflop;
        }
        else
        {
            // TRY TO RAISE
            if (legalActions.find(Action::Type::RAISE) != legalActions.end())
            {
                return {Action::Type::RAISE, getPostflopBetSize(handStrength, roundState, active, actionCategory)};
            }
            else if (legalActions.find(Action::Type::CALL) != legalActions.end())
            {
                std::cout << "Call after failed reraise" << std::endl;
                return {Action::Type::CALL};
            }
            else
            {
                numSelfChecks++;
                if (twoCheckBluff)
                {
                    std::cout << "Not actual two check bluff" << std::endl;
                    twoCheckBluff = false;
                }
                if (threeCheckBluff)
                {
                    std::cout << "Not actual three check bluff" << std::endl;
                    threeCheckBluff = false;
                }
                if (bountyBluff)
                {
                    std::cout << "Not actual bounty bluff" << std::endl;
                    bountyBluff = false;
                }
                std::cout << "Check after failed call and reraise" << std::endl;
                return {Action::Type::CHECK};
            }
        }
    }
};

/*
  Main program for running a C++ pokerbot.
*/
int main(int argc, char *argv[])
{
    srand(time(NULL));
    auto [host, port] = parseArgs(argc, argv);
    runBot<Bot>(host, port);
    return 0;
}
