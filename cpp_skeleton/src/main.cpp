#include <skeleton/actions.h>
#include <skeleton/constants.h>
#include <skeleton/runner.h>
#include <skeleton/states.h>
#include <iostream>
#include <array>
#include <time.h>

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

// -------------------------------------------------------------------
// A single Card class in C++
// -------------------------------------------------------------------
class Card
{
public:
    std::uint32_t code; // same 32-bit encoding from "poker.h"

    Card() : code(0) {}
    explicit Card(std::uint32_t c) : code(c) {}

    // Rank index: 0..12 (Deuce..Ace)
    int rank() const
    {
        return rankOfCard(code);
    }

    // Suit detection as in original logic:
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
};

// -------------------------------------------------------------------
// Deck class
//   - Creates the 52 cards with the exact encoding from original "poker.h"
//   - Shuffles them
// -------------------------------------------------------------------
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

    // Build the 52-card deck using the same bit logic as original "init_deck"
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

// -------------------------------------------------------------------
// Perfect-hash function "find_fast" from original logic
// -------------------------------------------------------------------
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

// -------------------------------------------------------------------
// Evaluate a 5-card hand (matching the "eval_5hand" logic).
//   - First check if all same suit => possibly flush or straight-flush
//   - Then check unique5[] => possibly straight or high card
//   - Otherwise multiply prime factors and do perfect-hash => other combos
// Returns an integer in [1..7462] (the "equivalence class").
// -------------------------------------------------------------------
unsigned short eval5Hand(const Card &c1,
                         const Card &c2,
                         const Card &c3,
                         const Card &c4,
                         const Card &c5)
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

// For printing final classification (matching value_str[] in poker.h)
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

// BOUNTY HYPERPARAMETERS
static const int roundsPerBounty = 25;
static const double bountyRatio = 1.5;
static const int bountyConstant = 10;

// GAME PARAMETERS
static const int numRounds = 1000;
static const int startingStack = 400;
static const int bigBlind = 2;
static const int smallBlind = 1;

using namespace pokerbots::skeleton;

struct Bot
{

    int totalRounds = 1;
    int timesBetPreflop = 0;

    std::unordered_map<std::string, int> preflopDict = {
        {"AAo", 1}, {"KKo", 2}, {"QQo", 3}, {"JJo", 4}, {"TTo", 5}, {"99o", 6}, {"88o", 7}, {"AKs", 8}, {"77o", 9}, {"AQs", 10}, {"AJs", 11}, {"AKo", 12}, {"ATs", 13}, {"AQo", 14}, {"AJo", 15}, {"KQs", 16}, {"KJs", 17}, {"A9s", 18}, {"ATo", 19}, {"66o", 20}, {"A8s", 21}, {"KTs", 22}, {"KQo", 23}, {"A7s", 24}, {"A9o", 25}, {"KJo", 26}, {"55o", 27}, {"QJs", 28}, {"K9s", 29}, {"A5s", 30}, {"A6s", 31}, {"A8o", 32}, {"KTo", 33}, {"QTs", 34}, {"A4s", 35}, {"A7o", 36}, {"K8s", 37}, {"A3s", 38}, {"QJo", 39}, {"K9o", 40}, {"A5o", 41}, {"A6o", 42}, {"Q9s", 43}, {"K7s", 44}, {"JTs", 45}, {"A2s", 46}, {"QTo", 47}, {"44o", 48}, {"A4o", 49}, {"K6s", 50}, {"K8o", 51}, {"Q8s", 52}, {"A3o", 53}, {"K5s", 54}, {"J9s", 55}, {"Q9o", 56}, {"JTo", 57}, {"K7o", 58}, {"A2o", 59}, {"K4s", 60}, {"Q7s", 61}, {"K6o", 62}, {"K3s", 63}, {"T9s", 64}, {"J8s", 65}, {"33o", 66}, {"Q6s", 67}, {"Q8o", 68}, {"K5o", 69}, {"J9o", 70}, {"K2s", 71}, {"Q5s", 72}, {"T8s", 73}, {"K4o", 74}, {"J7s", 75}, {"Q4s", 76}, {"Q7o", 77}, {"T9o", 78}, {"J8o", 79}, {"K3o", 80}, {"Q6o", 81}, {"Q3s", 82}, {"98s", 83}, {"T7s", 84}, {"J6s", 85}, {"K2o", 86}, {"22o", 87}, {"Q2s", 87}, {"Q5o", 89}, {"J5s", 90}, {"T8o", 91}, {"J7o", 92}, {"Q4o", 93}, {"97s", 80}, {"J4s", 95}, {"T6s", 96}, {"J3s", 97}, {"Q3o", 98}, {"98o", 99}, {"87s", 75}, {"T7o", 101}, {"J6o", 102}, {"96s", 103}, {"J2s", 104}, {"Q2o", 105}, {"T5s", 106}, {"J5o", 107}, {"T4s", 108}, {"97o", 109}, {"86s", 110}, {"J4o", 111}, {"T6o", 112}, {"95s", 113}, {"T3s", 114}, {"76s", 80}, {"J3o", 116}, {"87o", 117}, {"T2s", 118}, {"85s", 119}, {"96o", 120}, {"J2o", 121}, {"T5o", 122}, {"94s", 123}, {"75s", 124}, {"T4o", 125}, {"93s", 126}, {"86o", 127}, {"65s", 128}, {"84s", 129}, {"95o", 130}, {"53s", 131}, {"92s", 132}, {"76o", 133}, {"74s", 134}, {"65o", 135}, {"54s", 87}, {"85o", 137}, {"64s", 138}, {"83s", 139}, {"43s", 140}, {"75o", 141}, {"82s", 142}, {"73s", 143}, {"93o", 144}, {"T2o", 145}, {"T3o", 146}, {"63s", 147}, {"84o", 148}, {"92o", 149}, {"94o", 150}, {"74o", 151}, {"72s", 152}, {"54o", 153}, {"64o", 154}, {"52s", 155}, {"62s", 156}, {"83o", 157}, {"42s", 158}, {"82o", 159}, {"73o", 160}, {"53o", 161}, {"63o", 162}, {"32s", 163}, {"43o", 164}, {"72o", 165}, {"52o", 166}, {"62o", 167}, {"42o", 168}, {"32o", 169}};

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

        std::cout << "\nRound " << totalRounds << " starting" << std::endl;
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

        char bounty_rank = previousState->bounties[active]; // your bounty rank

        // The following is a demonstration of accessing illegal information (will not work)
        char opponent_bounty_rank = previousState->bounties[1 - active]; // attempting to grab opponent's bounty rank
        if (myBountyHit)
        {
            std::cout << "I hit my bounty of " << bounty_rank << "!" << std::endl;
        }
        if (oppBountyHit)
        {
            std::cout << "Opponent hit their bounty of " << opponent_bounty_rank << "!" << std::endl;
        }

        totalRounds++;
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

    int noIllegalRaises(int myBet, RoundStatePtr roundState)
    {
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
        auto boardCards = roundState->deck;              // the board cards
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
        int myBet = 0;

        std::vector<std::string> myCards(roundState->hands[active].begin(), roundState->hands[active].end());
        std::string newCards = categorize_cards(myCards);

        int handStrength = preflopDict.find(newCards)->second;

        std::cout << "Hand strength: " << handStrength << std::endl;
        std::cout << "Times bet preflop: " << timesBetPreflop << std::endl;

        if (!bigBlind && timesBetPreflop == 0)
        {
            if (handStrength < 26)
            {
                std::cout << "3x raise from sb" << std::endl;
                timesBetPreflop++;
                myBet = 3 * pot;
                return {Action::Type::RAISE, noIllegalRaises(myBet, roundState)};
            }
            else if (handStrength < 88)
            {
                std::cout << "2x raise from sb" << std::endl;
                timesBetPreflop++;
                myBet = 2 * pot;
                return {Action::Type::RAISE, noIllegalRaises(myBet, roundState)};
            }
            else
            {
                std::cout << "Fold from sb" << std::endl;
                return {Action::Type::FOLD};
            }
        }
        else if (bigBlind && timesBetPreflop == 0)
        {
            if (handStrength < 5 || (handStrength < 88 && pot <= 20))
            {
                timesBetPreflop++;
                myBet = 2 * pot;

                if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                {
                    std::cout << "2x raise from bb" << std::endl;
                    return {Action::Type::RAISE, noIllegalRaises(myBet, roundState)};
                }
                else if (legalActions.find(Action::Type::CALL) != legalActions.end())
                {
                    std::cout << "Call after failed 2x raise from bb" << std::endl;
                    return {Action::Type::CALL};
                }
                else
                {
                    std::cout << "No legal actions found" << std::endl;
                }
            }
            else if (oppPip == 2 && handStrength < 60 && (rand() / double(RAND_MAX)) < 0.69)
            {
                timesBetPreflop++;
                myBet = 2 * pot;

                if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                {
                    std::cout << "2x random raise from bb" << std::endl;
                    return {Action::Type::RAISE, noIllegalRaises(myBet, roundState)};
                }
                else
                {
                    std::cout << "Check after failed 2x random raise from bb" << std::endl;
                    return {Action::Type::CHECK};
                }
            }
            else if (handStrength < (88 + 1 - pow((oppPip - 2) / 198.0, 1.0 / 3.0) * (88 + 1 - 5)) && oppPip <= 200)
            {
                if (legalActions.find(Action::Type::CALL) != legalActions.end())
                {
                    std::cout << "Call from bb otherwise" << std::endl;
                    return {Action::Type::CALL};
                }
                else
                {
                    std::cout << "Check after failed call from bb otherwise" << std::endl;
                    return {Action::Type::CHECK};
                }
            }
            else
            {
                if (legalActions.find(Action::Type::CHECK) != legalActions.end())
                {
                    std::cout << "Check from bb otherwise" << std::endl;
                    return {Action::Type::CHECK};
                }
                else
                {
                    std::cout << "Fold after failed check from bb otherwise" << std::endl;
                    return {Action::Type::FOLD};
                }
            }
        }
        else
        {
            if (handStrength < 5)
            {
                timesBetPreflop++;
                myBet = 2*pot;

                if (legalActions.find(Action::Type::RAISE) != legalActions.end())
                {
                    std::cout << "2x reraise from raise" << std::endl;
                    return {Action::Type::RAISE, noIllegalRaises(myBet, roundState)};
                }
                else if (legalActions.find(Action::Type::CALL) != legalActions.end())
                {
                    std::cout << "Call after failed 2x reraise from raise" << std::endl;
                    return {Action::Type::CALL};
                }
                else
                {
                    std::cout << "No legal actions found" << std::endl;
                }
            }
            else if (handStrength < (67 - pow((oppPip - 2) / 398.0, 1.0 / 3.0) * 61))
            {
                if (legalActions.find(Action::Type::CALL) != legalActions.end())
                {
                    std::cout << "Call from reraise otherwise" << std::endl;
                    return {Action::Type::CALL};
                }
                else
                {
                    std::cout << "Check after failed call from reraise otherwise" << std::endl;
                    return {Action::Type::CHECK};
                }
            }
            else
            {
                if (legalActions.find(Action::Type::CHECK) != legalActions.end())
                {
                    std::cout << "Check from reraise otherwise" << std::endl;
                    return {Action::Type::CHECK};
                }
                else
                {
                    std::cout << "Fold after failed check from reraise otherwise" << std::endl;
                    return {Action::Type::FOLD};
                }
            }
        }

        std::cout << "Reraise HAVENT FINISHED YET" << std::endl;
        return {Action::Type::CHECK};
    }

    /*
      Where the magic happens - your code should implement this function.
      Called any time the engine needs an action from your bot.

      @param gameState The GameState object.
      @param roundState The RoundState object.
      @param active Your player's index.
      @return Your action.
    */

    std::pair<Action, int> getPostflopAction(RoundStatePtr gameState, int active)
    {
        return {{Action::Type::FOLD}, 0};
    }

    Action getAction(GameInfoPtr gameState, RoundStatePtr roundState, int active)
    {
        auto legalActions =
            roundState->legalActions();                  // the actions you are allowed to take
        int street = roundState->street;                 // 0, 3, 4, or 5 representing pre-flop, flop, turn, or river respectively
        auto myCards = roundState->hands[active];        // your cards
        auto boardCards = roundState->deck;              // the board cards
        int myPip = roundState->pips[active];            // the number of chips you have contributed to the pot this round of betting
        int oppPip = roundState->pips[1 - active];       // the number of chips your opponent has contributed to the pot this round of betting
        int myStack = roundState->stacks[active];        // the number of chips you have remaining
        int oppStack = roundState->stacks[1 - active];   // the number of chips your opponent has remaining
        int continueCost = oppPip - myPip;               // the number of chips needed to stay in the pot
        int myContribution = STARTING_STACK - myStack;   // the number of chips you have contributed to the pot
        int oppContribution = STARTING_STACK - oppStack; // the number of chips your opponent has contributed to the pot
        char myBounty = roundState->bounties[active];    // your current bounty rank

        if (street == 0)
        {
            return getPreflopAction(roundState, active);
        }
        else
        {
            std::pair<Action, int> postflopAction = getPostflopAction(roundState, active);
        }

        // if (street == 3)
        // {

        //   std::cout << "STREET 3!!!!!!!" << std::endl;

        //   std::cout << "Board: ";
        //   for (const auto &boardCard : boardCards)
        //   {
        //     std::cout << boardCard << " ";
        //   }

        //   std::cout << std::endl;

        //   std::cout << "Cards: ";
        //   for (const auto &card : myCards)
        //   {
        //     std::cout << card << " ";
        //   }
        //   std::cout << std::endl;

        //   Card c1(generateCardCodeFromString(myCards[0]));
        //   std::cout << "myCard1: " << c1.rankChar() << c1.suitChar() << std::endl;
        //   Card c2(generateCardCodeFromString(myCards[1]));
        //   std::cout << "myCard2: " << c2.rankChar() << c2.suitChar() << std::endl;

        //   Card c3(generateCardCodeFromString(boardCards[0]));
        //   Card c4(generateCardCodeFromString(boardCards[1]));
        //   Card c5(generateCardCodeFromString(boardCards[2]));

        //   unsigned short val = eval5Hand(c1, c2, c3, c4, c5);
        //   int rankType = handRank(val);

        //   std::cout << "Evaluator numeric value: " << val << "\n"
        //             << "Classification: " << HAND_TYPE_NAME[rankType] << std::endl;
        // }

        int minCost = 0, maxCost = 0;
        std::array<int, 2> raiseBounds = {0, 0};
        if (legalActions.find(Action::Type::RAISE) != legalActions.end())
        {
            raiseBounds = roundState->raiseBounds(); // the smallest and largest numbers of chips for a legal bet/raise
            minCost = raiseBounds[0] - myPip;        // the cost of a minimum bet/raise
            maxCost = raiseBounds[1] - myPip;        // the cost of a maximum bet/raise
        }

        if (legalActions.find(Action::Type::RAISE) != legalActions.end())
        {
            if (rand() % 2 == 0)
            {
                return {Action::Type::RAISE, raiseBounds[0]};
            }
        }
        if (legalActions.find(Action::Type::CHECK) != legalActions.end())
        {
            return {Action::Type::CHECK};
        }
        if (rand() % 4 == 0)
        {
            return {Action::Type::FOLD};
        }
        return {Action::Type::CALL};
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
