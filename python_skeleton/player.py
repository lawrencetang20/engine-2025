from skeleton.actions import FoldAction, CallAction, CheckAction, RaiseAction
from skeleton.states import GameState, TerminalState, RoundState
from skeleton.states import NUM_ROUNDS, STARTING_STACK, BIG_BLIND, SMALL_BLIND
from skeleton.bot import Bot
from skeleton.runner import parse_args, run_bot
import eval7
import random


class Player(Bot):
    '''
    A pokerbot.
    '''

    def __init__(self):
        '''
        Called when a new game starts. Called exactly once.

        Arguments:
        Nothing.

        Returns:
        Nothing.
        '''
        self.value_count = 0
        self.bounty_bluff_count = 0
        self.semibluff_count = 0
        self.bluff_count = 0
        self.total_one_check = 0
        self.count_one_check = 0
        self.total_two_check = 0
        self.count_two_check = 0
        # this is just ordering our preflop hands 
        self.raises_this_street = 0
        self.all_hands =  {'AAo':1,'KKo':2,'QQo':3,'JJo':4,'TTo':5,'99o':6,'88o':7,'AKs':8,'77o':9,'AQs':10,'AJs':11,'AKo':12,'ATs':13,
                             'AQo':14,'AJo':15,'KQs':16,'KJs':17,'A9s':18,'ATo':19,'66o':20,'A8s':21,'KTs':22,'KQo':23,'A7s':24,'A9o':25,'KJo':26,
                             '55o':27,'QJs':28,'K9s':29,'A5s':30,'A6s':31,'A8o':32,'KTo':33,'QTs':34,'A4s':35,'A7o':36,'K8s':37,'A3s':38,'QJo':39,
                             'K9o':40,'A5o':41,'A6o':42,'Q9s':43,'K7s':44,'JTs':45,'A2s':46,'QTo':47,'44o':48,'A4o':49,'K6s':50,'K8o':51,'Q8s':52,
                             'A3o':53,'K5s':54,'J9s':55,'Q9o':56,'JTo':57,'K7o':58,'A2o':59,'K4s':60,'Q7s':61,'K6o':62,'K3s':63,'T9s':64,'J8s':65,
                             '33o':66,'Q6s':67,'Q8o':68,'K5o':69,'J9o':70,'K2s':71,'Q5s':72,'T8s':73,'K4o':74,'J7s':75,'Q4s':76,'Q7o':77,'T9o':78,
                             'J8o':79,'K3o':80,'Q6o':81,'Q3s':82,'98s':83,'T7s':84,'J6s':85,'K2o':86,'22o':87,'Q2s':87,'Q5o':89,'J5s':90,'T8o':91,
                             'J7o':92,'Q4o':93,'97s':80,'J4s':95,'T6s':96,'J3s':97,'Q3o':98,'98o':99,'87s':75,'T7o':101,'J6o':102,'96s':103,'J2s':104,
                             'Q2o':105,'T5s':106,'J5o':107,'T4s':108,'97o':109,'86s':110,'J4o':111,'T6o':112,'95s':113,'T3s':114,'76s':80,'J3o':116,'87o':117,
                             'T2s':118,'85s':119,'96o':120,'J2o':121,'T5o':122,'94s':123,'75s':124,'T4o':125,'93s':126,'86o':127,'65s':128,'84s':129,'95o':130,
                             '53s':131,'92s':132,'76o':133,'74s':134,'65o':135,'54s':87,'85o':137,'64s':138,'83s':139,'43s':140,'75o':141,'82s':142,'73s':143,
                             '93o':144,'T2o':145,'T3o':146,'63s':147,'84o':148,'92o':149,'94o':150,'74o':151,'72s':152,'54o':153,'64o':154,'52s':155,'62s':156,
                             '83o':157,'42s':158,'82o':159,'73o':160,'53o':161,'63o':162,'32s':163,'43o':164,'72o':165,'52o':166,'62o':167,'42o':168,'32o':169,
                             }
        self.agressor = False
    def convert_to_hand_key(self, cards):
        # Define rank hierarchy
        rank_order = {
            '2': 2, '3': 3, '4': 4, '5': 5, '6': 6, '7': 7,
            '8': 8, '9': 9, 'T': 10, 'J': 11, 'Q': 12, 'K': 13, 'A': 14
        }
        
        if len(cards) != 2:
            raise ValueError("Exactly two cards must be provided.")
        
        card1, card2 = cards
        # Extract rank and suit, ensuring correct casing
        try:
            rank1, suit1 = card1[0].upper(), card1[1].lower()
            rank2, suit2 = card2[0].upper(), card2[1].lower()
        except IndexError:
            raise ValueError("Each card must have a rank and a suit, e.g., 'Ah', 'Ts'.")
        
        # Validate ranks and suits
        valid_ranks = set(rank_order.keys())
        valid_suits = {'s', 'h', 'd', 'c'}
        
        if rank1 not in valid_ranks or rank2 not in valid_ranks:
            raise ValueError(f"Invalid rank detected. Valid ranks are {valid_ranks}.")
        if suit1 not in valid_suits or suit2 not in valid_suits:
            raise ValueError(f"Invalid suit detected. Valid suits are {valid_suits}.")
        
        # Check for a pair
        if rank1 == rank2:
            return f"{rank1}{rank2}o"  # Pairs are always marked as offsuit ('o')
        
        # Determine the higher and lower ranks
        if rank_order[rank1] > rank_order[rank2]:
            high_rank, low_rank = rank1, rank2
        else:
            high_rank, low_rank = rank2, rank1
        
        # Determine if the hand is suited or offsuit
        suited = 's' if suit1 == suit2 else 'o'
        
        return f"{high_rank}{low_rank}{suited}"
            

    def handle_new_round(self, game_state, round_state, active):
        '''
        Called when a new round starts. Called NUM_ROUNDS times.

        Arguments:
        game_state: the GameState object.
        round_state: the RoundState object.
        active: your player's index.

        Returns:
        Nothing.
        '''
        self.my_stack_beginning_of_this_round = 401
        self.num_checks= 0 
        self.one_check, self.two_check = False, False
        self.agressor = False
        my_bankroll = game_state.bankroll  # the total number of chips you've gained or lost from the beginning of the game to the start of this round
        game_clock = game_state.game_clock  # the total number of seconds your bot has left to play this game
        self.round_num = game_state.round_num  # the round number from 1 to NUM_ROUNDS
        my_cards = round_state.hands[active]  # your cards
        big_blind = bool(active)  # True if you are the big blind
        my_bounty = round_state.bounties[active]  # your current bounty rank
        if self.round_num == 999:
            print(f'value: {self.value_count}, bounty bluff: {self.bounty_bluff_count}, semibluff: {self.semibluff_count}, total bluff: {self.bluff_count}')
 

        self.bounty_dict = {} # set all bounties to 0 -- we want to play these very aggressively, for now even more than AA
        for hand in self.all_hands.keys():
            if my_bounty in hand:
                self.bounty_dict[hand] = 0
            else: 
                self.bounty_dict[hand] = self.all_hands[hand]
        self.have_bounty = False
        if self.bounty_dict[self.convert_to_hand_key(my_cards)]==0:
            self.have_bounty = True
        print(f'have bounty: {self.have_bounty}')
        print(f'round {self.round_num}')
        print(f'my hand: {my_cards}')
    def handle_round_over(self, game_state, terminal_state, active):
        '''
        Called when a round ends. Called NUM_ROUNDS times.

        Arguments:
        game_state: the GameState object.
        terminal_state: the TerminalState object.
        active: your player's index.

        Returns:
        Nothing.
        '''
        my_delta = terminal_state.deltas[active]  # your bankroll change from this round
        #previous_state = terminal_state.previous_state  # RoundState before payoffs
        #street = previous_state.street  # 0, 3, 4, or 5 representing when this round ended
        #my_cards = previous_state.hands[active]  # your cards
        #opp_cards = previous_state.hands[1-active]  # opponent's cards or [] if not revealed
        #opponent_bounty = teriminal_state.bounty_hits # True if opponent hit bounty
        if self.one_check:
            self.total_one_check += my_delta
            self.count_one_check += 1
        if self.two_check:
            self.total_two_check += my_delta
            self.count_two_check += 1
        self.round_num = game_state.round_num
        if self.round_num == NUM_ROUNDS:
            print(f'num one check bluffs: {self.count_one_check}')
            print(f'delta when one check bluffing: {self.total_one_check}')
            print(f'num two check bluffs: {self.count_two_check}')
            print(f'delta when two check bluffing: {self.total_two_check}')
            
        
    def get_action(self, game_state, round_state, active):
        '''
        Where the magic happens - your code should implement this function.
        Called any time the engine needs an action from your bot.

        Arguments:
        game_state: the GameState object.
        round_state: the RoundState object.
        active: your player's index.

        Returns:
        Your action.
        '''

        big_blind = bool(active)
        legal_actions = round_state.legal_actions()  # the actions you are allowed to take
        street = round_state.street  # 0, 3, 4, or 5 representing pre-flop, flop, turn, or river respectively
        my_cards = round_state.hands[active]  # your cards
        board_cards = round_state.deck[:street]  # the board cards
        my_pip = round_state.pips[active]  # the number of chips you have contributed to the pot this round of betting
        opp_pip = round_state.pips[1-active]  # the number of chips your opponent has contributed to the pot this round of betting
        my_stack = round_state.stacks[active]  # the number of chips you have remaining
        opp_stack = round_state.stacks[1-active]  # the number of chips your opponent has remaining
        continue_cost = opp_pip - my_pip  # the number of chips needed to stay in the pot
        my_bounty = round_state.bounties[active]  # your current bounty rank
        my_contribution = STARTING_STACK - my_stack  # the number of chips you have contributed to the pot
        opp_contribution = STARTING_STACK - opp_stack  # the number of chips your opponent has contributed to the pot
        preflop_value = self.bounty_dict[self.convert_to_hand_key(my_cards)]
        my_bankroll = game_state.bankroll
        pot_size = my_contribution + opp_contribution + my_pip* 2
        if RaiseAction in legal_actions:
           min_raise, max_raise = round_state.raise_bounds()  # the smallest and largest numbers of chips for a legal bet/raise
           min_cost = min_raise - my_pip  # the cost of a minimum bet/raise
           max_cost = max_raise - my_pip  # the cost of a maximum bet/raise
        max_raise = my_pip + my_stack
        max_cost = my_pip + my_stack
        if street==3 and not big_blind:
            if opp_pip == 0:
                print('opponent checked flop')
                self.num_checks += 1
            else:
                self.num_checks = min(self.num_checks, 0)
        elif street==3 and big_blind:
            self.current = opp_contribution
        elif street == 4 and big_blind:
            if opp_contribution==self.current:
                print('opponent checked flop')
                self.current = opp_contribution
                self.num_checks += 1
            else:
                min(self.num_checks, 0)
        elif street == 4 and not big_blind:
            if opp_pip == 0:
                print('opponent checked turn')
                self.num_checks += 1
            else:
                min(self.num_checks, 0)
        elif street == 5 and big_blind:
            if opp_contribution==self.current:
                print('opponent checked turn')
                self.current = opp_contribution
                self.num_checks += 1
            else:
                min(self.num_checks, 0)
        elif street == 5 and not big_blind:
            if opp_pip == 0:
                print('opponent checked river')
                self.num_checks = 1
            else:
                min(self.num_checks, 0)





        self.my_stack_beginning_of_this_round = STARTING_STACK-(my_contribution - my_pip)
        

        
        

        # if we have enough money to last the rest of the game we can reasonably give up on all future hands
        rounds_left = NUM_ROUNDS-self.round_num + 1 
        if rounds_left * 12 < my_bankroll:
            if CheckAction() in legal_actions:
                return CheckAction()
            return FoldAction()
        if rounds_left * 5 < my_bankroll and rounds_left > 100:
            if CheckAction() in legal_actions:
                return CheckAction()
            return FoldAction()
            
        # preflop logic
        if street == 0:
            if RaiseAction not in legal_actions: 
                if preflop_value<=6:
                    return CallAction()
                return FoldAction()
            if not bool(active) and my_contribution == 1: # if we are dealer and haven't acted yet
                if preflop_value <= 30: # juice up the pot with nutted hands but try to stay less exploitable
                    self.agressor = True
                    amount = random.choice([8,9,10,11,9,10,11,11,11])
                    return RaiseAction(amount)
                elif preflop_value <= 70:
                    amount = random.choice([8,9,10,8,9,10,11])
                    return RaiseAction(amount)
                elif preflop_value <= 110:
                    amount = random.choice([8,8,8,8,9,10,11])
                    return RaiseAction(amount)
                else:
                    return FoldAction()
            elif bool(active) and my_contribution == 2: # if we are the big blind and haven't acted yet
                if opp_contribution <= 9: #if opponent raises small
                    if preflop_value <= 30:
                        amount = min(400,int(4 * opp_contribution))
                        self.agressor = True
                        return RaiseAction(amount)
                    elif CheckAction in legal_actions:
                            return CheckAction()
                    elif preflop_value <= 110:
                        return CallAction()
                    else: 
                        return FoldAction()
                elif opp_contribution <= 15:
                    if preflop_value <= 10:
                        amount = min(int(2.5 * opp_contribution),400)
                        self.agressor = True
                        return RaiseAction(amount)
                    elif CheckAction in legal_actions:
                            return CheckAction()
                    elif preflop_value <= 60:
                        return CallAction()
                    else: 
                        if CheckAction in legal_actions:
                            return CheckAction()
                        return FoldAction()
                elif opp_contribution >= 15:
                    if preflop_value<=10:
                        return CallAction()
                    return FoldAction()
            else: #any other situations: dealer raises massive or 3 bets, 4 bets, etc. 
                opp_range_size = int(2 + 400 / opp_contribution) #estimate of how many hands are in the opponent's range that raises us
                if preflop_value <= opp_range_size / 3: #reraise with nutted hands 
                    rand = random.random()
                    if rand >= .8:
                        amount = min(400, int(3*opp_contribution))
                        self.agressor = True
                        return RaiseAction(amount)
                    else: 
                        return CallAction()
                elif preflop_value <= opp_range_size * 1.25:
                    return CallAction()
                else:
                    return FoldAction()
        
        # flop logic
        elif street == 3 or street == 4:
            if my_pip == 0:
                self.raises_this_street = 0
            
            if my_stack == 0:
                return CheckAction()
            # sample 200 times, one item from my range and one item from the opponent's range to determine who's range is stronger. 
            # want to bet if my range is stronger. Can bluff a ton 
            self.flop_picked_up_bounty = False
            for i in board_cards: 
                if i == my_bounty:
                    self.flop_picked_up_bounty = True
            print(f'flop: {board_cards}')
            my_hand = my_cards + board_cards 
            my_hand_eval7 = [eval7.Card(s) for s in my_hand]
            NUM_SAMPLES = 200
            win_count = 0
            nut_count = 0
            for _ in range(NUM_SAMPLES):
                deck = eval7.Deck()
                for card in my_hand_eval7:
                    deck.cards.remove(card)
                deck.shuffle()
                opp_hole = deck.deal(2)
                rest = deck.deal(5-street)
                my_hand = my_hand_eval7 + rest
                my_value = eval7.evaluate(my_hand)

                opp_hand = [eval7.Card(s) for s in board_cards] + opp_hole + rest
                opp_value = eval7.evaluate(opp_hand)
                if eval7.handtype(my_value) in ['Straight', 'Flush', 'Straight Flush', 'Royal Flush'] and eval7.handtype(opp_value) not in ['Straight', 'Flush', 'Straight Flush', 'Royal Flush']:
                    nut_count += 1 
                if my_value > opp_value:
                    win_count += 1
            equity = win_count / NUM_SAMPLES #indicator of a strong hand
            draw_pct = nut_count / NUM_SAMPLES
            print(f'equity vs random cards = {equity}')
            print(f'draw_pct = {draw_pct}')
            pot_size = opp_contribution+my_contribution+opp_pip*2

            if RaiseAction in legal_actions and CallAction not in legal_actions: # 1. Should we one-check or two-check bluff?
                if self.num_checks == 2 and equity<.5: 
                    pct = random.choice([.6,.7,.8])
                    print("TWO CHECK BLUFF")
                    self.num_checks = -10000 #so we don't check bluff again this hand
                    amount = max(min_raise, round(min(pot_size * pct, max_cost)))
                    self.two_check = True
                    self.agressor = True
                    return RaiseAction(amount)
                elif self.num_checks == 1 and equity < .5:
                    if random.random() > .6:
                        pct = random.choice([.6,.8,.7])
                        print("ONE CHECK BLUFF")
                        self.num_checks = -10000 #so we don't check bluff again this hand
                        amount = max(min_raise, round(min(pot_size * pct, max_cost)))
                        self.one_check = True
                        self.agressor = True
                        return RaiseAction(amount)
            if my_pip == 0 and opp_pip == 0 and self.agressor: #2. Preflop raiser with no bet yet
                if equity > .65: #value
                    self.value_count += 1
                    pct = random.choice([.5,.6,.7])
                    amount = round(min(pot_size * pct, max_cost))
                    self.raises_this_street += 1 
                    self.agressor = True
                    return RaiseAction(amount)
                elif draw_pct > .1: #semi bluff
                    self.semibluff_count += 1
                    pct = random.choice([.5,.6,.7])
                    amount = round(min(pot_size * pct, max_raise))
                    self.raises_this_street += 1 
                    self.agressor = True
                    return RaiseAction(round(amount))
                elif self.have_bounty or self.flop_picked_up_bounty: #bounty bluff
                    rand = random.random()
                    if equity + rand > .9:
                        pct = random.choice([.4,.5,.6])
                        amount = round(min(pot_size * pct, max_cost))
                        self.raises_this_street += 1 
                        self.bounty_bluff_count += 1
                        self.agressor = True
                        return RaiseAction(round(amount))
                    else: 
                        return CheckAction()
                else: #total bluff or check? 
                    rand = random.random()
                    if rand > .7:
                        pct = random.choice([.2,.3,.4])
                        amount = max(round(min(pot_size * pct, max_cost)), 2)
                        self.raises_this_street += 1 
                        self.bluff_count +=1 
                        self.agressor = True
                        return RaiseAction(round(amount))
                    else: 
                        return CheckAction()
                
            elif my_pip == 0 and opp_pip == 0 and not self.agressor: # We are BB but not agressor, always check
                return CheckAction()

            else: # either my_pip>0 or opp_pip>0
  
                pot_size = opp_contribution+my_contribution+opp_pip*2
                amount_raise = int(min(max_cost, .75 *  pot_size))
                size_bet = continue_cost / (2*my_contribution)
                x = 0
                y = 0
                if self.flop_picked_up_bounty:
                    x = .15
                    y = .05

                if size_bet<.6:
                    if equity>.85-x:
                        if self.raises_this_street == 0:
                            self.value_count += 1
                            if RaiseAction in legal_actions:
                                self.agressor = True
                                return RaiseAction(amount_raise)
                            self.agressor = False
                            return CallAction()
                        if self.raises_this_street == 1:
                            if equity > .95:
                                self.value_count += 1
                                if RaiseAction in legal_actions:
                                    self.agressor = True
                                    return RaiseAction(amount_raise)
                                self.agressor = False
                                return CallAction()
                    elif draw_pct>.15-y:
                        self.semibluff_count +=1 
                        if RaiseAction in legal_actions:
                                self.agressor = True
                                return RaiseAction(amount_raise)
                        self.agressor = False
                        return CallAction()
                    elif equity>.45:
                        return CallAction()
                        self.agressor = False
                    if size_bet < .3 and equity > .25:
                        self.agressor = False
                        return CallAction()
                    return FoldAction()
                elif size_bet<1:
                    if equity>.925-x:
                        self.value_count +=1 
                        if RaiseAction in legal_actions:
                            self.agressor = True
                            return RaiseAction(amount_raise)
                        self.agressor = False
                        return CallAction()
                    elif draw_pct>.25-1.5*y:
                        self.semibluff_count += 1
                        if RaiseAction in legal_actions:
                            return RaiseAction(amount_raise)
                        self.agressor = False
                        return CallAction()
                    elif equity>.6:
                        self.agressor = False
                        return CallAction()
                    return FoldAction()
                else:
                    if equity>.925-x:
                        self.value_count +=1 
                        if RaiseAction in legal_actions:
                            self.agressor = True
                            return RaiseAction(amount_raise)
                        self.agressor = False
                        return CallAction()
                    elif draw_pct>.25-1.5*y:
                        self.semibluff_count +=1
                        if RaiseAction in legal_actions:
                            self.agressor = True
                            return RaiseAction(amount_raise)
                        self.agressor = False
                        return CallAction()
                    elif equity>.8:
                        self.agressor = False
                        return CallAction()
                    return FoldAction()

        elif street == 5:
            if my_pip == 0:
                self.raises_this_street = 0
            if my_stack == 0:
                return CheckAction()
            pot_size = opp_contribution+my_contribution+opp_pip*2
            if RaiseAction in legal_actions and CallAction not in legal_actions:
                if self.num_checks == 2: 
                    pct = random.choice([.6,.7,.8])
                    print("TWO CHECK BLUFF")
                    amount = max(min_raise, round(min(pot_size * pct, max_cost)))
                    self.two_check = True
                    return RaiseAction(amount)
                elif self.num_checks == 1:
                    if random.random() > .6:
                        pct = random.choice([.6,.7,.8])
                        print("ONE CHECK BLUFF")
                        amount = max(min_raise, round(min(pot_size * pct, max_cost)))
                        self.one_check = True
                        return RaiseAction(amount)
                    

            # sample 200 times, one item from my range and one item from the opponent's range to determine who's range is stronger. 
            # want to bet if my range is stronger. Can bluff a ton 
            self.flop_picked_up_bounty = False
            for i in board_cards: 
                if i == my_bounty:
                    self.flop_picked_up_bounty = True
            my_hand = my_cards + board_cards 
            my_hand_eval7 = [eval7.Card(s) for s in my_hand]
            NUM_SAMPLES = 200
            win_count = 0
            for _ in range(NUM_SAMPLES):
                deck = eval7.Deck()
                for card in my_hand_eval7:
                    deck.cards.remove(card)
                deck.shuffle()
                my_hand = my_hand_eval7
                my_value = eval7.evaluate(my_hand)
                opp_hole = deck.deal(2)
                opp_hand = [eval7.Card(s) for s in board_cards] + opp_hole
                opp_value = eval7.evaluate(opp_hand)
                if my_value > opp_value:
                    win_count += 1
            equity = win_count / NUM_SAMPLES #indicator of a strong hand

            print(f'equity vs random cards = {equity}')
            additive = 0
            if my_stack <= 360:
                additive = .1
            if my_stack <= 300:
                additive = .2
            if my_stack <= 220:
                additive = .25
            if my_pip == 0 and opp_pip == 0 and self.agressor: #lead out betting
                if equity > .65+ additive: #value
                    self.value_count += 1
                    pct = random.choice([.5,.6,.7])
                    amount = round(min(pot_size * pct, max_cost))
                    self.raises_this_street += 1 
                    return RaiseAction(amount)

                elif self.have_bounty or self.flop_picked_up_bounty: #bounty bluff
                    rand = random.random()
                    if rand > .3:
                        pct = random.choice([.4,.5,.6])
                        amount = round(min(pot_size * pct, max_cost))
                        self.raises_this_street += 1 
                        self.bounty_bluff_count += 1
                        return RaiseAction(round(amount))
                    else: 
                        return CheckAction()
                else:
                    rand = random.random()
                    if rand > .7:
                        pct = random.choice([.2,.3,.4])
                        amount = max(round(min(pot_size * pct, max_cost)), 2)
                        self.raises_this_street += 1 
                        self.bluff_count +=1 
                        return RaiseAction(round(amount))
                    else: 
                        return CheckAction()

            elif my_pip == 0 and opp_pip == 0: #not agressor but in position
                if self.flop_picked_up_bounty or equity>.65:
                    pct = random.choice([.4,.5,.6])
                    amount = round(min(pot_size * pct, max_cost))
                    self.raises_this_street += 1 
                    return RaiseAction(amount)
                else:
                    return CheckAction()
            else: # reacting to a bet
                print(f'max cost = {max_cost}')
                pot_size = opp_contribution+my_contribution+opp_pip*2
                amount_raise = int(min(max_cost, .75 *  pot_size))
                print()
                print(amount_raise)
                print()
                size_bet = continue_cost / (2*my_contribution)
                x = 0
                y=0
                if self.flop_picked_up_bounty:
                    x = .15
                    y=.05
                
                if size_bet<.6:
                    if equity>.85-x:
                        if self.raises_this_street == 0:
                            self.value_count += 1
                            if RaiseAction in legal_actions:
                                return RaiseAction(amount_raise)
                            return CallAction()
                        if self.raises_this_street == 1:
                            if equity > .95:
                                self.value_count += 1
                                if RaiseAction in legal_actions:
                                    return RaiseAction(amount_raise)
                                return CallAction()
                    elif equity>.45:
                        return CallAction()
                    if size_bet < .3 and equity > .25:
                        return CallAction()
                    return FoldAction()
                elif size_bet<1:
                    if equity>.925-x:
                        self.value_count +=1 
                        if RaiseAction in legal_actions:
                            return RaiseAction(amount_raise)
                        return CallAction()
                    elif equity>.6:
                        return CallAction()
                    return FoldAction()
                else:
                    if equity>.925-x:
                        self.value_count +=1 
                        if RaiseAction in legal_actions:
                            return RaiseAction(amount_raise)
                        return CallAction()
                    elif equity>.8:
                        return CallAction()
                    return FoldAction()
            




                
                        



            

            

                            

                    






        else:
            if RaiseAction in legal_actions:
                if random.random() < 0.5:
                    return RaiseAction(min_raise)
            if CheckAction in legal_actions:  # check-call
                return CheckAction()
            if random.random() < 0.25:
                return FoldAction()
            return CallAction()


if __name__ == '__main__':
    run_bot(Player(), parse_args())